
var value = 0;

function init() {}

function add(count) public {
	value += count;
}

function increment() public {
	value++;
}

function get_value() public const {
	return value;
}

function reset() public {
	value = 0;
}

