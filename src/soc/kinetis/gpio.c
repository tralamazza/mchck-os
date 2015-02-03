#include <mchck.h>

GPIO_MemMapPtr
gpio_physgpio_from_pin(enum gpio_pin_id pin)
{
        switch (pin_port_from_pin(pin)) {
        case PIN_PORTA:
                return (PTA_BASE_PTR);
        case PIN_PORTB:
                return (PTB_BASE_PTR);
        case PIN_PORTC:
                return (PTC_BASE_PTR);
        case PIN_PORTD:
                return (PTD_BASE_PTR);
        case PIN_PORTE:
                return (PTE_BASE_PTR);
        default:
                return (NULL);
        }
}

void
gpio_dir(enum gpio_pin_id pin, enum gpio_dir dir)
{
        int pinnum = pin_physpin_from_pin(pin);
        GPIO_MemMapPtr pinp = gpio_physgpio_from_pin(pin);

        switch (dir) {
        case GPIO_OUTPUT:
                GPIO_PDDR_REG(pinp) |= 1 << pinnum;
                goto set_mux;
        case GPIO_INPUT:
                GPIO_PDDR_REG(pinp) &= ~(1 << pinnum);
        set_mux:
                pin_mode(pin, PIN_MODE_MUX_ALT1);
                break;
        case GPIO_DISABLE:
                pin_mode(pin, PIN_MODE_MUX_ANALOG);
                break;
        }
}

void
gpio_write(enum gpio_pin_id pin, enum gpio_pin_value val)
{
        BITBAND_BIT(GPIO_PDOR_REG(gpio_physgpio_from_pin(pin)), pin_physpin_from_pin(pin)) = val;
}

void
gpio_toggle(enum gpio_pin_id pin)
{
        GPIO_PTOR_REG(gpio_physgpio_from_pin(pin)) = 1 << pin_physpin_from_pin(pin);
}

enum gpio_pin_value
gpio_read(enum gpio_pin_id pin)
{
        return (BITBAND_BIT(GPIO_PDIR_REG(gpio_physgpio_from_pin(pin)), pin_physpin_from_pin(pin)));
}