

function init() {}

function cross_add(value) public {
	rcall("counter", "add", value);
}

function cross_increment() public {
	rcall("counter", "increment");
}

function cross_read_update() public {
	const value = rcall("counter", "get_value");
	if(value % 2 != 0) {
		rcall("counter", "increment");
	}
}
