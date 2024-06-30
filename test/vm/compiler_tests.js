
null;

{
	var test;
	if(test != null) {
		fail("empty var");
	}
}

function test1(a, b = 1) {
	return a + b;
}

if(test1(1) != 2) {
	fail("test1");
}

if(concat(to_string(1), to_string(2), to_string(3)) != "123") {
	fail("concat()", 1);
}

{
	var map = {};
	if(map.test != null) {
		fail("map.test != null");
	}
}
{
	var map = {};
	map.tmp = 123;
	if(map.tmp != 123) {
		fail("var to key assignment");
	}
}
{
	var map = {"test": 123};
	const tmp = map.test;
	if(tmp != 123) {
		fail("key to var assignment");
	}
	map.tmp = map.test;
	if(map.tmp != 123) {
		fail("key to key assignment");
	}
}

if(this.balance[bech32()] != 0) {
	fail("balance != 0");
}
