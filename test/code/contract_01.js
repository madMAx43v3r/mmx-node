
var owner;
var value = 0;

function init(owner_) {
	owner = owner_;
}

function add(count) public {
	if(count > 10) {
		fail("count > 10");
	}
	value += count;
}

function increment() public {
	value++;
}

function get_value() public const {
	return value;
}

function reset() public {
	if(this.user != owner) {
		fail("user != owner");
	}
	value = 0;
}

