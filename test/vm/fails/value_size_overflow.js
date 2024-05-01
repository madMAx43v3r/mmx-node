
var value = "0123456789ABCDEF";

for(var i = 0; i < 13; ++i) {
	value = concat(value, value);
}

value = null;
