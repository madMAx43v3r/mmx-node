
function func(a, b) { return; }

var map = {};

var test = [];

for(var i = 0, test = 0; i < 6; ++i, i = i + 1) {
	++test;
}

for(const value of test) {
	map = value;
}
