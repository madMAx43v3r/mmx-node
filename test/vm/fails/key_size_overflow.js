
var key = "0123456789ABCDEF";

for(var i = 0; i < 8; ++i) {
	key = concat(key, key);
}
key = concat(key, ".");

var map = {};
map[key] = 1;
