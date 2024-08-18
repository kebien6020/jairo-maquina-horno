#include <Arduino.h>
#include <RS485.h>
#include <errno.h>
#include <libmodbus/modbus-rtu.h>

auto tx = 17, de = 22, re = 21;
auto rs485 = RS485Class{Serial2, tx, de, re};
auto mb = modbus_new_rtu(&rs485, 9600, SERIAL_8N1);

auto print_errno(char const* ctx) -> void {
	printf("%s, errno: %d\n", ctx, errno);
}

auto halt() -> void {
	for (;;)
		;
}

auto setup() -> void {
	Serial.begin(115200);
	modbus_set_slave(mb, 2);
	modbus_set_debug(mb, true);
	modbus_connect(mb);
}

auto loop() -> void {
	auto temp_raw = uint16_t{0};
	modbus_read_input_registers(mb, 0x03E8, 1, &temp_raw);
	printf("temp_raw: %d\n", temp_raw);

	modbus_write_register(mb, 0x0000, 24);

	auto out1 = uint8_t{0};
	modbus_read_input_bits(mb, 0x0003, 1, &out1);

	printf("out1: %d\n", out1);
	delay(1000);
}
