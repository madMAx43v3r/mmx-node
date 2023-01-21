
function func(a, b) { return; }

var map = {};

var test = [];

for(var i = 0, test = 0; i < 6; ++i, i = i + 1) {
	++test;
}

for(const value of test) {
	map = value;
}

var owner;

function check_owner() {
    if (this.user != owner) {
        fail("user != owner", 1);
    }
    var tmp = to_string_bech32(this.user);
}
