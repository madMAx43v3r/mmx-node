
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

// for(var i = 0; i < 10; ++i) {
// 	if(i > 3) {
// 		break;
// 	}
// 	if(i > 3) {
// 		fail("for() break");
// 	}
// }

