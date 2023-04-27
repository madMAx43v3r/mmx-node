
if(1 + 1 != 2) {
	fail("add", 1);
}
if(1 - 1 != 0) {
	fail("sub", 1);
}
if(1 * 1 != 1) {
	fail("mul", 1);
}
if(1 / 1 != 1) {
	fail("div", 1);
}
if(1 % 1 != 0) {
	fail("mod", 1);
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
	fail("bech32", 4);
}






