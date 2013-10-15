#include <mchck.h>

#include <usb/usb.h>
#include <usb/cdc-acm.h>

static struct cdc_ctx cdc;

static void
new_data(uint8_t *data, size_t len)
{
	// ignore data
	cdc_read_more(&cdc);
}

static void
init_vcdc(int config)
{
	cdc_init(new_data, NULL, &cdc);
	cdc_set_stdout(&cdc);
}

static const struct usbd_device cdc_device =
	USB_INIT_DEVICE(0x2323,              /* vid */
		3,                   /* pid */
		u"mchck.org",        /* vendor */
		u"i2c test", /* product" */
		(init_vcdc,          /* init */
		 CDC)                /* functions */
	       );

static struct timeout_ctx t;

#define MPU6050_ADDR 0b1101000

struct acc_value {
    int16_t x_accel;
    int16_t y_accel;
    int16_t z_accel;
    int16_t temperature;
    int16_t x_gyro;
    int16_t y_gyro;
    int16_t z_gyro;
} ;

struct i2c_ctx {
	enum i2c_state {
		I2C_STATE_IDLE,
		I2C_STATE_RX,
		I2C_STATE_TX
	} state;

	uint8_t *cur_buf;
	int cur_buf_idx;
	unsigned int cur_buf_len;
} ctx;

void
i2c_init() {
	// turn on clock to I2C module
	SIM.scgc4.i2c0 = 1;

	// set I2C rate
	I2C0.f = (struct I2C_F) { .mult = I2C_MULT_1, .icr = 0x1B };	// XXX: check values, possibly determine dynamically

	// enable I2C and interrupt
	I2C0.c1.iicen = 1;
	I2C0.c1.iicie = 1;
	int_enable(IRQ_I2C0);
	
	// initialize state
	ctx.state = I2C_STATE_IDLE;
	ctx.cur_buf = NULL;
	ctx.cur_buf_idx = 0;
	ctx.cur_buf_len = 0;
}

static const enum gpio_pin_id led_pin = GPIO_PTC0;

// pretty straight implementation of page 1036 of the K20 Sub-Family Reference Manual
void I2C0_Handler(void) {
	gpio_toggle(led_pin);
	I2C0.s.iicif = 1;	// clear interrupt flag
	switch(ctx.state) {
		case I2C_STATE_IDLE:
			// we shouldn't get an interrupt when idle
			break;

		case I2C_STATE_RX:
			if(ctx.cur_buf_idx == -1) {		// if we haven't received a byte yet, the previous operation was the address transmit
				if(I2C0.s.rxak) {		// if no ack,
					I2C0.c1.mst = 0;	// generate STOP
				} else {
					// start the actual receiving
					I2C0.c1.tx = 0;
					ctx.cur_buf_idx = 0;
					volatile uint8_t dummy = I2C0.d;	// dummy read
				}
			} else {
				if(ctx.cur_buf_idx == ctx.cur_buf_len-1) {		// if last byte,
					I2C0.c1.mst = 0;				// generate STOP
					ctx.state = I2C_STATE_IDLE;
				} else if(ctx.cur_buf_idx == ctx.cur_buf_len-2) {	// if last-but-one byte,
					I2C0.c1.txak = 1;				// send acknowledge
				}

				ctx.cur_buf[ctx.cur_buf_idx++] = I2C0.d;		// store received data
			}
			break;

		case I2C_STATE_TX:
			if(ctx.cur_buf_idx == ctx.cur_buf_len-1) {	// if last byte,
				ctx.state = I2C_STATE_IDLE;
				I2C0.c1.mst = 0;			// generate STOP
			} else {
				if(I2C0.s.rxak)				// if no ack,
					I2C0.c1.mst = 0;		// generate STOP
				else
					I2C0.d = ctx.cur_buf[ctx.cur_buf_idx++];

			}
			break;
	}
}

void
i2c_send(uint8_t address, uint8_t *data, unsigned int len)
{
	// change ctx
	ctx.state = I2C_STATE_TX;
	ctx.cur_buf = data;
	ctx.cur_buf_idx = 0;
	ctx.cur_buf_len = len;

	// kick off transmission by sending address
	I2C0.c1.tx = 1;
	I2C0.c1.mst = 1;
	I2C0.d = (address << 1) | 0x00;	// LSB = 0 => write transaction
}

void
i2c_receive(uint8_t address, uint8_t* data, unsigned int len)
{
	// change ctx
	ctx.state = I2C_STATE_RX;
	ctx.cur_buf = data;
	ctx.cur_buf_idx = -1;	// first byte will be address
	ctx.cur_buf_len = len;
	
	// kick off reception by sending address
	I2C0.c1.tx = 1;
	I2C0.c1.mst = 1;
	I2C0.d = (address << 1) | 0x01;	// LSB = 1 => read transaction
}

static struct acc_value values;

void read_mpu6050(void *data) {
	// receive whoami data
	uint8_t whoami_req[1] = {0x75};
	uint8_t whoami = 0;

	i2c_send(MPU6050_ADDR, whoami_req, 1);
	while(ctx.state != I2C_STATE_IDLE)
		;
	i2c_receive(MPU6050_ADDR, &whoami, 1);
	while(ctx.state != I2C_STATE_IDLE)
		;
	printf("whoami: %x\r\n", whoami);

	timeout_add(&t, 1000, read_mpu6050, NULL);
}

void
main(void)
{
	SIM.scgc5.portb = 1;

	PORTB.pcr[2].mux = PCR_MUX_ALT2;
	PORTB.pcr[3].mux = PCR_MUX_ALT2;

	gpio_dir(led_pin, GPIO_OUTPUT);
	pin_mode(led_pin, PIN_MODE_DRIVE_HIGH);

	timeout_init();
        usb_init(&cdc_device);
	i2c_init();

	timeout_add(&t, 1000, read_mpu6050, NULL);
	sys_yield_for_frogs();
}
