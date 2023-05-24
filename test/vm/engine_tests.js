
if(1 + 1 != 2) {
	fail("add", 1);
}
if(1 - 1 != 0) {
	fail("sub", 1);
}
if(1 * 1 != 1) {
	fail("mul", 1);
}
if(1337 * 1337 != 1787569) {
	fail("mul", 2);
}
if(340282366920938463463374607431768211455 * 340282366920938463463374607431768211455 != 115792089237316195423570985008687907852589419931798687112530834793049593217025) {
	fail("mul", 3);
}

if(3 / 2 != 1) {
	fail("div", 1);
}
if(13371337 / 1337 != 10001) {
	fail("div", 2);
}
if(340282366920938463463374607431768211455 / 1337 != 254511867554927796158096191048442940) {
	fail("div", 3);
}
if(115792089237316195423570985008687907853269984665640564039457584007913129639935 / 1337 != 86605900701059233675071791330357447908204924955602516110289890806217748421) {
	fail("div", 4);
}
if(3 / 1 != 3) {
	fail("div 3 / 1");
}
if(13371337 / 1024 != 13057) {
	fail("div 13371337 / 1024");
}
for(var i = 1; i < 100; ++i) {
	if((11 * i) / i != 11) {
		fail(concat("div (11 * i) / i: ", to_string(i)));
	}
}

if(3 % 2 != 1) {
	fail("mod", 1);
}
if(13371337 % 1337 != 0) {
	fail("mod", 2);
}
if(340282366920938463463374607431768211455 % 1337 != 675) {
	fail("mod", 3);
}
if(115792089237316195423570985008687907853269984665640564039457584007913129639935 % 1337 != 1058) {
	fail("mod", 4);
}
if(3 % 1 != 0) {
	fail("mod 3 % 1");
}
if(13371337 % 1024 != 969) {
	fail("mod 13371337 % 1024");
}
for(var i = 1; i < 100; ++i) {
	if((11 * i) % i != 0) {
		fail(concat("mod (11 * i) % i: ", to_string(i)));
	}
}

if((0xFF00FF ^ 0xFF00FF) != 0) {
	fail("xor", 1);
}
if((0xFF00FF & 0xFFFFFF) != 0xFF00FF) {
	fail("and", 1);
}
if((0xFF00FF | 0) != 0xFF00FF) {
	fail("or", 1);
}
if((~0xFF00FF & 0xFFFFFF) != 0x00FF00) {
	fail("not", 1);
}

{
	var res = 0;
	for(var i = 0; i < 10; ++i) {
		res += 2;
	}
	if(res != 20) {
		fail("for", 1);
	}
}
{
	var res = 0;
	for(var i of [1, 2, 3]) {
		res += i;
	}
	if(res != 6) {
		fail("for", 2);
	}
}

if(0x100 != 256) {
	fail("hex", 1);
}
if(0b100 != 4) {
	fail("bin", 1);
}
if(uint("0b01011101010001") != 0b01011101010001) {
	fail("uint(bin)", 1);
}
if(uint(111) != 111) {
	fail("uint(dec)", 1);
}
if(uint("111820312618873087563030836565815045174850059154595088905299609727287843170569") != 111820312618873087563030836565815045174850059154595088905299609727287843170569) {
	fail("uint(dec)", 2);
}
if(uint("0x0123456789ABCDEF") != 0x0123456789ABCDEF) {
	fail("uint(hex)", 1);
}
if(uint("0x0123456789abcdef") != 0x0123456789abcdef) {
	fail("uint(hex)", 2);
}
if(uint("0x09292D77A8DF8E790D467F458B29591C1AFB11F0676CC7ABBB778C60D90D38F7") != 0x09292D77A8DF8E790D467F458B29591C1AFB11F0676CC7ABBB778C60D90D38F7) {
	fail("uint(hex)", 3);
}
if(uint("0b1001") != 9) {
	fail("uint(bin)", 1);
}
if(bech32("mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev") != 0) {
	fail("bech32", 1);
}
if(bech32("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf") != 0xF7380DD9608C77BBABC76C67F011FB1A1C59298B457F460D798EDFA8772D2909) {
	fail("bech32", 2);
}
if(to_string(1337) != "1337") {
	fail("to_string(1337)");
}
if(to_string(18446744073709551615) != "18446744073709551615") {
	fail("to_string(18446744073709551615)");
}
if(to_string_hex(0x01234567ABCDE) != "1234567abcde") {
	fail("to_string_hex(0x01234567ABCDE)");
}
if(to_string_hex(0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855) != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
	fail("to_string_hex(0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)");
}
if(to_string(binary("ABCDEF")) != "414243444546") {
	fail("binary(ABCDEF)");
}
if(to_string(binary_hex("0123456789ABCDEF")) != "0123456789ABCDEF") {
	fail("binary_hex(0123456789ABCDEF)");
}
if(to_string(binary_hex("0x0123456789ABCDEF")) != "0123456789ABCDEF") {
	fail("binary_hex(0x0123456789ABCDEF)");
}
if(concat("A", "BC", "D") != "ABCD") {
	fail("concat()", 1);
}
if(sha256("") != binary_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")) {
	fail("sha256()", 1);
}
if(sha256("abc") != binary_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) {
	fail("sha256()", 2);
}
if(ecdsa_verify(
		binary_hex("F3D6C7817001F48946270CD00CE1EA2105F470F65DA66C1FBCAADE3E7CAB964A"),
		binary_hex("02054191D5103F7A04B2E5B53AF253A6476F11DDCDB3B4134D74CFD642969B082E"),
		binary_hex("C05F13C85BDDFBB0A198697FAAC6973AA008AA45544FB7AB586B636405F73F215237D508C6FAE90C24BE424584640F824864B6FF3F2B99FB2104BBD5743E8E39")) != true)
{
	fail("ecdsa_verify", 1);
}
if(ecdsa_verify(
		binary_hex("A3D6C7817001F48946270CD00CE1EA2105F470F65DA66C1FBCAADE3E7CAB964A"),
		binary_hex("02054191D5103F7A04B2E5B53AF253A6476F11DDCDB3B4134D74CFD642969B082E"),
		binary_hex("C05F13C85BDDFBB0A198697FAAC6973AA008AA45544FB7AB586B636405F73F215237D508C6FAE90C24BE424584640F824864B6FF3F2B99FB2104BBD5743E8E39")) != false)
{
	fail("ecdsa_verify", 2);
}





