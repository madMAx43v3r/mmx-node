
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


