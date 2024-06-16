
{}

{
}

if(false) {
	fail("false");
}
if(!true) {
	fail("!");
}
if(!(!false)) {
	fail("!!");
}
if(true && false) {
	fail("true && false");
}
if(false && true) {
	fail("false && true");
}
if(!(true || false)) {
	fail("true || false");
}
if(!(false || true)) {
	fail("false || true");
}
if(!(true && true)) {
	fail("true && true");
}
if(!(true || true)) {
	fail("true && true");
}
if(false || false) {
	fail("false || false");
}
if(false && false) {
	fail("false && false");
}
if(false ^ false) {
	fail("false ^ false");
}
if(true ^ true) {
	fail("true ^ true");
}
if(!(false ^ true)) {
	fail("false ^ true");
}
if(!(true ^ false)) {
	fail("false ^ true");
}
if(true) {
	// pass
} else {
	fail("else");
}
if(false) {
	fail("else-if");
} else if(false) {
	fail("else-if");
} else if(false) {
	fail("else-if");
} else {
	// pass
}

if(!(1 > 0)) {
	fail("1 > 0");
}
if(!(1 >= 0)) {
	fail("1 >= 0");
}
if(!(1 >= 1)) {
	fail("1 >= 1");
}
if(!(0 < 1)) {
	fail("0 < 1");
}
if(!(0 <= 1)) {
	fail("0 <= 1");
}
if(!(1 <= 1)) {
	fail("1 <= 1");
}

if((1 >> 0) != 1) {
	fail("1 >> 0");
}
if((2 >> 1) != 1) {
	fail("2 >> 1");
}
if((1 << 0) != 1) {
	fail("1 << 0");
}
if((1 << 1) != 2) {
	fail("1 << 1");
}

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
if(18446744073709551615 * 18446744073709551615 != 340282366920938463426481119284349108225) {
	fail("mul", 3);
}
if(340282366920938463463374607431768211455 * 340282366920938463463374607431768211455 != 115792089237316195423570985008687907852589419931798687112530834793049593217025) {
	fail("mul", 4);
}
if(86605900701059233675071791330357447908204924955602516110289890806217748420 * 1337 != 115792089237316195423570985008687907853269984665640564039457584007913129637540) {
	fail("mul", 5);
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
if(115792089237316195423570985008687907853269984665640564039457584007913129639935 / 340282366920938463463374607431768211455 != 340282366920938463463374607431768211457) {
	fail("div", 5);
}
if(115792089237316195423570985008687907853269984665640564039457584007913129639935 / 86605900701059233675071791330357447908204924955602516110289890806217748420 != 1337) {
	fail("div", 6);
}
if((3 >> 0) != 3) {
	fail("3 >> 0");
}
if(3 / 1 != 3) {
	fail(concat("div 3 / 1 => ", to_string(3 / 1)));
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
if(uint("") != 0) {
	fail("uint('')");
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
if(bech32("mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev") != binary_hex("0000000000000000000000000000000000000000000000000000000000000000")) {
	fail("bech32", 1);
}
if(bech32("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf") != binary_hex("09292D77A8DF8E790D467F458B29591C1AFB11F0676CC7ABBB778C60D90D38F7")) {
	fail("bech32", 2);
}
if(to_string_bech32(binary_hex("09292D77A8DF8E790D467F458B29591C1AFB11F0676CC7ABBB778C60D90D38F7")) != "mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf") {
	fail("bech32", 3);
}
if(bech32("MMX") != bech32()) {
	fail("bech32", 4);
}
if(bech32() != binary_hex("0000000000000000000000000000000000000000000000000000000000000000")) {
	fail("bech32", 5);
}
if(bech32(null) != bech32()) {
	fail("bech32", 6);
}
if(to_string(1337) != "1337") {
	fail("to_string(1337)");
}
if(to_string(18446744073709551615) != "18446744073709551615") {
	fail("to_string(18446744073709551615)");
}
if(to_string(340282366920938463463374607431768211455) != "340282366920938463463374607431768211455") {
	fail("to_string(340282366920938463463374607431768211455)");
}
if(to_string_hex(0x01234567ABCDE) != "1234567abcde") {
	fail("to_string_hex(0x01234567ABCDE)");
}
if(to_string_hex(0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855) != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
	fail("to_string_hex(0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)");
}
if(to_string_bech32(bech32("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf")) != "mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf") {
	fail("to_string_bech32(mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf)");
}
if(to_string(binary("ABCDEF")) != "ABCDEF") {
	fail("binary(ABCDEF)");
}
if(to_string_hex(binary("AB")) != "4142") {
	fail("binary(AB)");
}
if(to_string_hex(binary_hex("0123456789ABCDEF")) != "0123456789ABCDEF") {
	fail("binary_hex(0123456789ABCDEF)");
}
if(to_string_hex(binary_hex("0x0123456789ABCDEF")) != "0123456789ABCDEF") {
	fail("binary_hex(0x0123456789ABCDEF)");
}
if(concat("A", "BC", "D") != "ABCD") {
	fail("concat()", 1);
}
if(memcpy("ABC", 1) != "A") {
	fail(concat("memcpy(ABC, 1): '", memcpy("ABC", 1), "'"));
}
if(memcpy("ABC", 2, 1) != "BC") {
	fail("memcpy(ABC, 2, 1)");
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
if(size("123") != 3) {
	fail("size('123')");
}
if(size(binary("123")) != 3) {
	fail("size(binary('123'))");
}
if(min(1, 2, 3) != 1) {
	fail("min()");
}
if(max(1, 2, 3) != 3) {
	fail("max()");
}
if(typeof(null) != 0) {
	fail("typeof(null)");
}
if(typeof(false) != 1) {
	fail("typeof(false)");
}
if(typeof(true) != 2) {
	fail("typeof(true)");
}
if(typeof(1) != 4) {
	fail("typeof(1)");
}
if(typeof("") != 5) {
	fail("typeof('')");
}
if(typeof(binary("")) != 6) {
	fail("typeof(binary)");
}
if(typeof([]) != 7) {
	fail("typeof([])");
}
if(typeof({}) != 8) {
	fail("typeof({})");
}
if(deref(clone(1)) != 1) {
	fail("deref(clone(1)) != 1");
}
if(bool(null) != false) {
	fail("bool(null)");
}
if(bool(false) != false) {
	fail("bool(false)");
}
if(bool(true) != true) {
	fail("bool(true)");
}
if(bool(0) != false) {
	fail("bool(0)");
}
if(bool(10) != true) {
	fail("bool(10)");
}
if(bool("") != false) {
	fail("bool('')");
}
if(bool("0") != true) {
	fail("bool('0')");
}
if(bool(binary("")) != false) {
	fail("bool(binary(''))");
}
if(bool(binary("0")) != true) {
	fail("bool(binary('0')");
}
if(bool([]) != true) {
	fail("bool([])");
}
if(bool({}) != true) {
	fail("bool({})");
}
if(uint(null) != 0) {
	fail("uint(null)");
}
if(uint(false) != 0) {
	fail("uint(false)");
}
if(uint(true) != 1) {
	fail("uint(true)");
}
if(uint_hex("aabbc") != 0xaabbc) {
	fail("uint_hex('aabbc')");
}
if(uint(binary(0xABCDEF0)) != 0xABCDEF0) {
	fail("uint(binary(0xABCDEF0))");
}
if(uint(binary(binary(0xABCDEF0))) != 0xABCDEF0) {
	fail("uint(binary(binary(0xABCDEF0)))");
}
if(uint_le(binary_le(0xABCDEF0)) != 0xABCDEF0) {
	fail("uint_le(binary_le(0xABCDEF0))");
}
if(bech32(binary(bech32("MMX"))) != bech32("MMX")) {
	fail("bech32(binary(bech32(MMX)))");
}
if([] == []) {
	fail("[] == []");
}
if({} == {}) {
	fail("[] == []");
}
if(to_string("test") != "test") {
	fail("to_string(test)");
}

__nop();

{
	var array = [];
	if(size(array) != 0) {
		fail("size(array) != 0");
	}
	push(array, 11);
	if(array[0] != 11) {
		fail("array[0] != 11");
	}
	if(size(array) != 1) {
		fail("size(array) != 1");
	}
	array[0] = 12;
	{
		var tmp = clone(array);
		if(tmp[0] != 12) {
			fail("clone: array[0] != 12");
		}
		pop(tmp);
	}
	if(pop(array) != 12) {
		fail("pop(array) != 12");
	}
	if(size(array) != 0) {
		fail("size(array) != 0");
	}
	if(array[35345345345] != null) {
		fail("array[35345345345] != null");
	}
}
{
	var map = {};
	if(map.field != null) {
		fail("map.field != null");
	}
	map.field = 1337;
	if(get(map, "field") != 1337) {
		fail("map.field != 1337");
	}
	map.field++;
	if(map.field != 1338) {
		fail("map.field != 1338");
	}
	set(map, "field", 1339);
	{
		var tmp = clone(map);
		if(tmp.field != 1339) {
			fail("clone: tmp.field != 1339");
		}
		erase(tmp, "field");
	}
	if(map.field != 1339) {
		fail("map.field != 1339");
	}
	erase(map, "field");
	if(map.field != null) {
		fail("after erase: map.field != null");
	}
	{
		var tmp = clone(map);
		if(tmp.field != null) {
			fail("clone: tmp.field != null");
		}
	}
}
{
	var map = {};
	map[0] = true;
	map[false] = true;
	map[true] = true;
	map["test"] = true;
	map[binary("bin123")] = true;
	var tmp0 = [];
	map[tmp0] = true;
	var tmp1 = {};
	map[tmp1] = true;
	
	if(map[0] == null) {
		fail("map[0] == null");
	}
	if(map[false] == null) {
		fail("map[false] == null");
	}
	if(map[true] == null) {
		fail("map[true] == null");
	}
	if(map["test"] == null) {
		fail("map['test'] == null");
	}
	if(map[binary("bin123")] == null) {
		fail("map[bin123] == null");
	}
	if(map[tmp0] == null) {
		fail("map[tmp0] == null");
	}
	if(map[tmp1] == null) {
		fail("map[tmp1] == null");
	}
}
{
	var test = {
		field: "value",
		"field1": 123
	};
	if(test.field != "value") {
		fail("test.field");
	}
	if(test.field1 != 123) {
		fail("test.field1");
	}
	var test2 = {};
	test2.field = test.field;
	if(test2.field != test.field) {
		fail("test2.field");
	}
}
{
	var test = null;
	if(test.field != null) {
		fail("null.field != null");
	}
}
{
	var test = concat([1, 2], [3, 4], [5]);
	if(size(test) != 5) {
		fail("concat: size(test) != 5");
	}
	for(var i = 0; i < 5; ++i) {
		if(test[i] != i + 1) {
			fail("test[i] != i + 1");
		}
	}
}
{
	var test = {
		field: {
			field: 1337
		}
	};
	var test2 = test;
	test = null;
	if(test2.field.field != 1337) {
		fail("test2.field.field != 1337");
	}
}
{
	var test = 1;
	delete(test);
	test = 1;
	if(test != 1) {
		fail("use after delete()");
	}
}
{
	var test = [1, 2, 3];
	pop(test);
	push(test, 3);
	if(test[2] != 3) {
		fail("test[2] != 3");
	}
}
{
	var test = [1, 2, 3];
	var test2 = deref(test);
	if(size(test2) != 3) {
		fail("size(test2) != 3");
	}
	for(var i = 0; i < size(test2); ++i) {
		if(test2[i] != i + 1) {
			fail("test2[i] != i + 1");
		}
	}
	pop(test2);
	pop(test2);
	if(size(test) != 3) {
		fail("size(test) != 3");
	}
	for(var i = 0; i < size(test); ++i) {
		if(test[i] != i + 1) {
			fail("test[i] != i + 1");
		}
	}
}
{
	var test = [null, true, false, 1, "test", [], {}];
	if(test[0] != null) {
		fail("test[0] != null");
	}
	if(test[1] != true) {
		fail("test[1] != true");
	}
	if(test[2] != false) {
		fail("test[2] != false");
	}
	if(test[3] != 1) {
		fail("test[3] != 1");
	}
	if(test[4] != "test") {
		fail("test[4] != test");
	}
}
{
	var map = {};
	const key = bech32("mmx1cxn9nan8xyw3zflxheaf2c2mrzgexzp33j9rwqmxw7ed3ut09wqsr06jmq");
	map[key] = 1337;
	if(map[key] != 1337) {
		fail("map[key] != 1337 (key = bech32())");
	}
}
{
	const array = [1, 2, 3];
	const tmp = array;
	push(tmp, 4);
	if(array[3] != 4) {
		fail("array[3] != 4");
	}
}
{
	const object = {"foo": {}};
	const foo = object.foo;
	foo.bar = true;
	if(object.foo.bar != true) {
		fail("object.foo.bar != true");
	}
}



