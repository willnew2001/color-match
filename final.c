#include <u.h>
#include <libc.h>

/* Way of storing colors for the 2 bytes per pixel mode */
typedef struct color_s {
	uchar one;
	uchar two;
	uchar three;
} color_t;

void cmdW(uchar cmd);
void dataW(uchar* cmd, int n);
void setWindow(int startX, int endX, int startY, int endY);
void drawRect(int startX, int endX, int startY, int endY, color_t color);
void screenInit(uchar col_mode);

int spi_fd;
int gpio_fd;

void
cmdW(uchar cmd)
{
	fprint(gpio_fd, "set 25 0");
	pwrite(spi_fd, &cmd, 1, 0);
}

void
dataW(uchar* data, int n)
{
	fprint(gpio_fd, "set 25 1");
	pwrite(spi_fd, data, n, 0);
	fprint(gpio_fd, "set 25 0");
}

void
setWindow(int startX, int endX, int startY, int endY)
{
	uchar buf[32];

	/* Col set */
	cmdW(0x2a);

	buf[0] = startX >> 8;
	buf[1] = startX & 0xff;
	buf[2] = endX >> 8;
	buf[3] = endX & 0xff;
	dataW(buf, 4);

	/* Row set */
	cmdW(0x2b);

	buf[0] = startY >> 8;
	buf[1] = startY & 0xff;
	buf[2] = endY >> 8;
	buf[3] = endY & 0xff;
	dataW(buf, 4);
}

void
drawRect(int startX, int endX, int startY, int endY, color_t color)
{
	uchar buf[32];
	int pix_count = 0;

	/* Set up area to update */
	setWindow(startX, endX, startY, endY);

	/* Data set */
	cmdW(0x2c);

	/* Write pixels */
	fprint(gpio_fd, "set 25 1");
	for(int i = startX; i < endX+1; i++) {
		for(int j = startY; j < endY+1; j++){ //longer side
			if (pix_count % 2 == 0) {
				buf[0] = color.one;
				buf[1] = color.two;
				dataW(buf,2);
			} else {
				buf[0] = color.three;
				dataW(buf, 1);
			}
			pix_count++;
		}
	}
}

void
screenInit(uchar col_mode)
{
	uchar buf[32];

	/* software reset */
	cmdW(0x01);
	sleep(200);

	/* take out of sleep mode */
	cmdW(0x11);
	sleep(200);

	/* turn on display */
	cmdW(0x29);

	/* Normal display mode on */
	cmdW(0x13);

	/* Memory access control */
	cmdW(0x36);
	buf[0] = 0x00;
	dataW(buf, 1);

	/* Color mode to 18 bit */
	cmdW(0x3a);
	buf[0] = col_mode;
	dataW(buf, 1);
}

int
get_red_from_x(int x)
{
	int red;
	double inc;

	/* Standardize analog x between -81 and 81 */
	if (x < -81) x = -81;
	if (x > 81) x = 81;

	inc = 15.0/27.0;

	/* Red to purple range (only 15) */
	if (x >= 54) {
		red = 15;

	/* Purple to blue range (15-0) */
	} else if (x >= 27 && x < 54) {
		red = ((x%27)+1) * inc;

	/* Blue to green range (only 0) */
	} else if (x >= -27 && x < 27) {
		red = 0;

	/* Green to yellow range (0-15) */
	} else if (x >= -54 && x < -27) {
		red = (((abs(x)-1)%27)+1) * inc;

	/* Yellow to red range (only 15) */
	} else if (x < -54) {
		red = 15;
	}

	return red;
}

int
get_green_from_x(int x)
{
	int green;
	double inc;

	/* Standardize analog x between -81 and 81 */
	if (x < -81) x = -81;
	if (x > 81) x = 81;

	inc = 15.0/27.0;

	/* Red to blue range (only 0) */
	if (x >= 27) {
		green = 0;

	/* Blue to cyan range (0-15) */
	} else if (x >= 0 && x < 27) {
		green = ((x%27)+1) * inc;
		green = 15-green;

	/* Cyan to yellow range (only 15) */
	} else if (x >= -54 && x < 0) {
		green = 15;
	
	/* Yellow to red range (from 15-0) */
	} else if (x < -54) {
		green = (((abs(x)-1)%27)+1) * inc;
		green = 15 - green;
	}

	return green;
}
		
int
get_blue_from_x(int x)
{
	int blue;
	double inc;

	/* Standardize analog x between -81 and 81 */
	if (x < -81) x = -81;
	if (x > 81) x = 81;

	inc = 15.0/27.0;

	/* Red to purple range (0-15) */
	if (x >= 54) {
		if (x == 81) x = 80;
		blue = ((x%27)+1) * inc;
		blue = 15-blue;

	/* Purple to cyan range (only 15) */
	} else if (x >= 0 && x < 54) {
		blue = 15;

	/* Cyan to green range (15-0) */
	} else if (x >= -27 && x < 0) {
		blue = (((abs(x)-1)%27)+1) * inc;
		blue = 15 - blue;

	/* Green to red range (only) */
	} else if (x < -27) {
		blue = 0;
	}

	return blue;
}

int
get_tint_from_y(int y)
{
	int tint;

	/* Standardize analog y between -75 and 75 */	
	if (y < -75) y = -75;
	if (y > 75) y = 75;

	tint = (((double)y+75.0) / (75.0*2.0))*100;

	return 100-tint;
}

int
main()
{
	int i2c_fd;
	uchar buf[32];

	int i, round_count;
	int im_width = 40;
	int im_height = 48;

	int target_vals[3];
	int target_tint;
	int red_index;
	int green_index;
	int blue_index;
	int target_cols[3];
	uchar target_one, target_two, target_three;
	int tint, red_val, green_val, blue_val;
	uchar one, two, three;

	int accelX, accelY;
	int degX, degY;

	int counter;
	vlong time_start;
	vlong time_end;
	double elapsed;

	/* Set up GPIO */
	gpio_fd = open("/dev/gpio", ORDWR);
	if (gpio_fd < 0) {
		bind("#G", "/dev", MAFTER);
		gpio_fd = open("/dev/gpio", ORDWR);
		if (gpio_fd < 0) return 1;
	}
	fprint(gpio_fd, "function 25 out");

	/* Set up SPI */
	spi_fd = open("/dev/spi0", ORDWR);
	if (spi_fd < 0) {
		bind("#π", "/dev", MAFTER);
		spi_fd = open("/dev/spi0", ORDWR);
		if (spi_fd < 0) return 1;
	}

	/* Set up I2C */
	i2c_fd = open("/dev/i2c.52.data", ORDWR);
	if (i2c_fd < 0) {
		bind("#J52", "/dev", MAFTER);
		i2c_fd = open("/dev/i2c.52.data", ORDWR);
		if (i2c_fd < 0) return 1;
	}
	buf[0] = 0x40;
	buf[1] = 0x00;
	pwrite(i2c_fd, buf, 2, 0);
	sleep(10);

	/* Run screen initialization */
	screenInit(0x03);

	/* Set some colors */
	color_t bg = {0x00, 0x00, 0x00};
	color_t red = {0xf0, 0x0f, 0x00};

	/* Draw background */
	drawRect(0, 127, 0, 159, bg);

	/* Game loop */
	time_start = nsec();
	round_count = 5;
	for (i = 0; i < round_count; i++) {
		print("---------- ROUND %d ----------\n", i+1);
		/* Create const rectange */
		srand(time(0));

		target_tint = nrand(101);
		target_vals[0] = 15.0 * ((double)target_tint/100);
		target_vals[1] = (double)nrand(16) * ((double)target_tint/100);
	
		if (target_tint < 50) {
			target_vals[2] = (4.0/16.0) * ((double)(target_tint*2)/100.0);
		} else {
		target_vals[2] = (4.0/16.0) * ((double)((100-target_tint)*2)/100.0);
		}

		red_index = nrand(3);
		green_index = nrand(3);
		while (green_index == red_index) green_index = nrand(3);
		blue_index = nrand(3);
		while (blue_index == red_index || blue_index == green_index) blue_index = nrand(3);

		target_cols[0] = target_vals[red_index];
		target_cols[1] = target_vals[green_index];
		target_cols[2] = target_vals[blue_index];
	
		target_one = (target_cols[0] << 4) | target_cols[1];
		target_two = (target_cols[2] << 4) | target_cols[0];
		target_three = (target_one << 4) | (target_two >> 4);

		color_t target_color = {target_one, target_two, target_three};
	
		drawRect(40,40+im_height,39+im_width,39+(2*im_width),target_color);

		/* Match loop */
		counter = 0;
		while(1) {
			/* Fetch new data from nunchuck */
			buf[0] = 0x00;
			pwrite(i2c_fd, buf, 1, 0);
			sleep(2);

			/* Read new data */
			pread(i2c_fd, buf, 6, 0);
			sleep(2);

			accelX = (((buf[2]^0x17)+0x17) << 2) + ((((buf[5]^0x17)+0x17) >> 2) & 0x03);
			accelY = (((buf[3]^0x17)+0x17) << 2) + ((((buf[5]^0x17)+0x17) >> 4) & 0x03);
			degX = ((accelX-520)/220.0)*90;
			degY = ((accelY-500)/220.0)*90;

			/* If user flicks nunchuck, get new color */
			if (degX > 90 || degY > 90) {
				print("New color requested...\n");
				i--;
				break;
			}

			/* Create color based on accelerometer */
			tint = get_tint_from_y(degY);
			red_val = (double)get_red_from_x(degX) * ((double)tint/100);
			green_val = (double)get_green_from_x(degX) * ((double)tint/100);
			blue_val = (double)get_blue_from_x(degX) * ((double)tint/100);

			one = (red_val << 4) | green_val;
			two = (blue_val << 4) | red_val;
			three = (one << 4) | (two >> 4);

			color_t test_col = {one,two,three};
	
			/* If the color matches */
			if (abs(red_val-target_cols[0]) <= 1 && abs(green_val-target_cols[1]) <= 1 && abs(blue_val-target_cols[2]) <= 1) {
				print("HOOOOOLD\n");
				counter++;
			} else {
				print("Find the match!\n");
				counter = 0;
			}

			/* Draw current color */
			drawRect(40,40+im_height,39,39+im_width,test_col);

			/* If you've matched it for enough time */
			if (counter >= 10) {
				print("-------- MATCH MADE! --------\n");
				break;
			}
		}
	}

	time_end = nsec();
	print("-------- GAME OVER! ---------\n");
	drawRect(0, 127, 0, 159, red);
	drawRect(0, 127, 0, 159, bg);

	elapsed = (double)(time_end-time_start)/1000000000.0;

	print("\nYour time in seconds: %.3f\n", elapsed);
		
	close(spi_fd);
	close(gpio_fd);
	close(i2c_fd);
	return 0;
}
