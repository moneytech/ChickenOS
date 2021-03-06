#include <common.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>
#include <device/tty.h>
#include <device/video/vga.h>
#include <memory.h>
#include <mm/liballoc.h>

//FIXME: This should be console.c, tty should handle the termios stuff

//XXX MUST BE POWER OF TWO!
#define TTY_HISTORY_SIZE 128
#define TTY_HISTORY_MASK (TTY_HISTORY_SIZE - 1)
#define TTY_COUNT 4

typedef void (*tty_putchar_fn)(int c, int x, int y);

struct tty_state {
	bool foreground;
	int escape;
	char ansibuf[32];
	int bufpos;
	uint32_t x,y;
	uint32_t x_res, y_res;

	struct winsize winsize;
	struct termios termios;
	pid_t pgrp;
	uint32_t fg, bg;

	tty_putchar_fn putchar;
	//need buffers
	char **ringbuf;
	int r_top, r_cur;

	char **alt_ringbuf;
	int alt_r_top, alt_r_cur;
	uint32_t alt_x, alt_y;

	void *aux;
} *ttys[10];


char tty_history[TTY_COUNT][2][TTY_HISTORY_SIZE][80];

void tty_redraw(tty_t *tty)
{
	for(unsigned i = 0; i < tty->y_res; i++)
	{
		vga_put_line(i, tty->ringbuf[(tty->r_top + i) & 0x7F]);// & TTY_HISTORY_MASK]);
	}
}
void tty_clear_rows(tty_t *tty, int row, int count)
{
	(void)count;(void)row;
	for(int i = row; i < count; i++)
		kmemset(tty->ringbuf[(tty->r_top + i) & 0x7F], 0, tty->x_res);

	vga_clear_rows(0, 25);//tty->y_res);

}

void tty_escape(tty_t *tty, int c)
{
	char *p;
	char *d;
	switch(tty->escape)
	{
		case 1:
			tty->bufpos = 0;
			if(c == '[')
				tty->escape = 2;
			else
				tty->escape = 0;
			return;
		case 2:
			switch(c)
			{
				case 0 ... 63:
					tty->ansibuf[tty->bufpos++] = c & 0xff;
					return;
				case 'C':
					tty->ansibuf[tty->bufpos] = 0;
					int cmd = strtol(tty->ansibuf, NULL, 10);
					tty->x += cmd;
					break;
				case 'l':
				case 'h':
					if(tty->ansibuf[0] == '?')
					{
						tty->ansibuf[tty->bufpos] = 0;
						int cmd = strtol(tty->ansibuf + 1, NULL, 10);
						if(cmd == 1049)
						{
							//FIXME: make this a function
							char **buf_tmp = tty->ringbuf;
							tty->ringbuf = tty->alt_ringbuf;
							tty->alt_ringbuf = buf_tmp;
							int top = tty->r_top;
							tty->r_top = tty->alt_r_top;
							tty->alt_r_top = top;
							uint32_t alt = tty->x;
							tty->x = tty->alt_x;
							tty->alt_x = alt;
							alt = tty->y;
							tty->y = tty->alt_y;
							tty->alt_y = alt;

						}
						tty_redraw(tty);
					}
					break;
				case 'H':
					if(tty->bufpos == 0)
					{
						tty->x = 0;
						tty->y = 0;
					}else{
						tty->ansibuf[tty->bufpos] = 0;
						p = (char *)&tty->ansibuf;
						int x = strtol(tty->ansibuf, &p, 10);
						p++;
						int y = strtol(p, &d, 10);
						tty->x = y-1;
						tty->y = x-1;
					}
					vga_set_cursor_position(tty->x, tty->y);
					//HACK
					break;
				case 'J':
					if(tty->bufpos > 0)
					{
						tty->ansibuf[tty->bufpos] = 0;
						int cmd = strtol(tty->ansibuf, 0, 10);
						serial_printf("J %i %s\n", cmd, tty->ansibuf);

						if(cmd == 2)
							tty_clear_rows(tty, 0, tty->y_res);
						else if(cmd == 1)
							tty_clear_rows(tty, 0, tty->y);
						else if(cmd == 0)
							tty_clear_rows(tty, tty->y, tty->y_res - tty->y);


					}else
						tty_clear_rows(tty, 0, tty->y);
					tty_redraw(tty);
					break;
				case 'K':
				//	kmemset(tty->cur->line, 0, 80);
				//	vga_put_line(tty->y, tty->cur->line);

					//kmemsetw(&tty->videoram[tty->x*80], BLANK, 80);
					break;
				case 'm':
				/*	tty->x = 0;
					tty->y = 0;
					tty->top = 0;*/tty_redraw(tty);
					break;
				default:
					break;

			}
			break;
		default:
			tty->escape = 0;
	}

	tty->escape = 0;
}

void tty_scroll(tty_t *tty)
{
	kmemset(tty->ringbuf[tty->r_top & 0x7f], 0, 80);
	tty->r_top++;
	for(unsigned i = 0; i < tty->y; i++)
	{
		vga_put_line(i, tty->ringbuf[(tty->r_top + i) & 0x7F]);// & TTY_HISTORY_MASK]);
	}
	vga_put_line(tty->y, tty->ringbuf[(tty->r_top + tty->y) & 0x7f]);
}
void tty_putc(int c)
{
	tty_t *tty = ttys[0];
	tty_putchar(tty, c);

}
#define TTY_INSERT_BUF(tty, c) ((tty)->ringbuf[((tty)->r_top + (tty)->y) & 0x7f][(tty)->x] = c)
void tty_putchar(tty_t *tty, int c)
{
	if(tty->x >= tty->x_res)
	{
		tty->x = 0;
		tty->y++;
	}
	if(tty->y >= tty->y_res)
	{
		tty->y = tty->y_res - 1;
		tty->x = 0;
		tty_scroll(tty);
	}

	if(tty->escape)
	{
		tty_escape(tty, c);
		return;
	}

	switch(c)
	{
		//FIXME Doesn't handle backspace properly
		case 0x08:
			if(tty->x > 0)
				tty->x--;
			else if(tty->y > 0)
				tty->y--;
			TTY_INSERT_BUF(tty, ' ');
			vga_putchar(' ', tty->x, tty->y);
			break;
		case '\t':
			tty->x = (tty->x + 8) & ~(7);//Wat?
			break;
		case '\n':
			tty->x = 0;
			tty->y++;
			break;
		case '\r':
			tty->x = 0;
			break;
		case 7://?
			break;
		case 0x1b://ESC
			tty->escape = 1;
			break;

		default:
			TTY_INSERT_BUF(tty, c);
			vga_putchar(c, tty->x, tty->y);
			tty->x++;
	}

	vga_set_cursor_position(tty->x, tty->y);
}
int tty_getc(tty_t *tty)
{
	int c;
	while(tty->foreground == false);
	extern char kbd_getc();
	c = kbd_getc();
	return c;
}

size_t tty_getline(tty_t *tty, uint8_t *buf, size_t len)
{
	size_t ret = 0;
	int c = 0;
	serial_printf("tty_getline len %i\n", len);
	while(len--)
	{
		c = tty_getc(tty);
		serial_printf("Got char %c %x\n", c, c);
		if(c == 0x8)
		{
			buf[ret] = '\0';
			if((signed)ret > 0)
			{
				ret--;
			if(tty->termios.c_lflag & ECHO)
				tty_putchar(tty, c);

			}
		}
		else
		{
			if(tty->termios.c_lflag & ECHO)
				tty_putchar(tty, c);

			buf[ret] = c;
			ret++;
		}
		if(c == 0xa)
			break;
	}
	return ret;
}

//int tty_read(struct inode *inode, void *_buf, off_t off UNUSED, size_t count)?
int tty_read(dev_t dev, void *_buf, size_t count, off_t offset UNUSED)
{
	char *buf = _buf;
	(void)buf;
	int c;
	size_t ret = 0;
	int tty_num = MINOR(dev);
	tty_t *tty = ttys[tty_num];

	if (tty->termios.c_lflag & ICANON)
		ret = tty_getline(tty, _buf, count);
	else
	{
		while(count--)
		{
				c = tty_getc(tty);
				if(tty->termios.c_lflag & ECHO)
					tty_putchar(tty, c);

				if(c == 0xa)
				{
					if((tty->termios.c_iflag & INLCR) == 0)
					{
					//	c = '\r';
					}
				}
				buf[ret] = c;
				ret++;

				if(ret >= tty->termios.c_cc[VMIN])
					break;
		}
	}

	buf[ret] = 0;
	serial_printf("Reading %s %i %i\n", _buf, count, offset);
	return ret;
}


//int tty_write(struct inode *inode, void *_buf, off_t off UNUSED, size_t count)
//XXX: Incompatible type signature here, will have to fix in many places
int tty_write(dev_t dev, const void *_buf, size_t count, off_t offset UNUSED)
{
	int tty_num = MINOR(dev);
	const char *p = _buf;

	while(count--)
	{
		serial_putc(*p);
		tty_putchar(ttys[tty_num], *p++);
	}

	return p - (const char *)_buf;
}

int tty_ioctl(dev_t dev, int request, char *args )
{
	int tty_num = MINOR(dev);
	tty_t *tty = ttys[tty_num];

	switch(request)
	{
		case TCGETS:
			if(args != NULL)
				kmemcpy(args, &tty->termios, sizeof(struct termios));
			else
				return -EFAULT;
			tty_termios_print(&tty->termios);
			break;
		case TCSETSF:
		case TCSETS:
			if(args != NULL)
				kmemcpy(&tty->termios, args, sizeof(struct termios));
			else
				return -EFAULT;
			break;
		case TIOCGWINSZ:
			if(args != NULL)
				kmemcpy(args, &tty->winsize, sizeof(struct winsize));
			else
				return -EFAULT;
			break;
		case TIOCGPGRP:
			if(args != NULL)
				*(int *)args = tty->pgrp;
			else
				return -EFAULT;
			break;
		case TIOCSPGRP:
			if(args != NULL)
				tty->pgrp = *(int *)args;
			else
				return -EFAULT;
			break;
		default:
			printf("request %x @ tty %i\n", request, tty_num);
	}

	return 0;//-EINVAL;
}


struct termios default_termios = {
	.c_iflag = ICRNL,
	.c_oflag = OPOST | ONLCR,
	.c_cflag = 0,
	.c_lflag = IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
	.c_line = 0,
	.c_cc = {
		0x03, 0x1C, 0x7F, 0x15,
		0x04, 0x00, 0x01, 0x00,
		0x11, 0x13, 0x1A, 0xFF,
		0x12, 0x0F, 0x17, 0x16,
		0xFF
		},
};

void termios_init(struct termios *termios)
{
	kmemcpy(termios, &default_termios, sizeof(*termios));

	termios->c_cflag |= B38400;
}

#include <mm/vm.h>
void tty_init(struct kernel_boot_info *info)
{
//	struct tty_buffer *p;

	for(int i = 0; i < 1; i++)
	{
		ttys[i] = kcalloc(sizeof(tty_t), 1);

		ttys[i]->winsize.ws_col = info->x_res;
		ttys[i]->winsize.ws_row = info->y_res;

		ttys[i]->x_res = info->x_res;
		ttys[i]->y_res = info->y_res;

		ttys[i]->ringbuf = kcalloc(TTY_HISTORY_SIZE, sizeof(char *));
		for(unsigned j = 0; j < TTY_HISTORY_SIZE; j++)
			ttys[i]->ringbuf[j] = &tty_history[i][0][j][0];//[80];

		//FIXME Remove all the old linked list stuff
/*		ttys[i]->buffer = kcalloc(sizeof *p, 1);
		ttys[i]->top = ttys[i]->buffer;
		p = ttys[i]->buffer;
		ttys[i]->cur = p;

		for(int j = 0; j < 100; j++)
		{
			p->line = kcalloc(info->x_res, 1);
			p->next = kcalloc(sizeof *p, 1);
			p = p->next;
		}

		p->line = kcalloc(info->x_res, 1);
		p->next = ttys[i]->top;
*/

		ttys[i]->pgrp = 0;

		termios_init(&ttys[i]->termios);
	}

	ttys[0]->foreground = true;

	device_register(FILE_CHAR, 0x500, tty_read, tty_write, tty_ioctl);
	device_register(FILE_CHAR, 0x800, tty_read, tty_write, tty_ioctl);
	//device_register(FILE_CHAR, 0x500, console_read, console_write, console_ioctl);
}

