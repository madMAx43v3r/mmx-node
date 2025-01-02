
interface counter;

function init() {}

function cross_add(value) public {
	counter.add(value);
// 	rcall("counter", "add", value);
}

function cross_increment() public {
	counter.increment();
// 	rcall("counter", "increment");
}

function cross_read_update() public {
	const value = counter.get_value();
// 	const value = rcall("counter", "get_value");
	if(value % 2 != 0) {
		counter.increment();
// 		rcall("counter", "increment");
	}
}
