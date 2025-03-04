
null;
assert(true);
assert(true, "test");
assert(1 == 1);

{
	var test;
	assert(test == null);
}

function test1(a, b = 1) {
	return a + b;
}

assert(test1(1) == 2);
assert(test1(1, 2) == 3);

assert(concat(string(1), string(2), string(3)) == "123");

{
	var map = {};
	assert(map.test == null);
}
{
	var map = {};
	map.tmp = 123;
	assert(map.tmp == 123);
}
{
	var map = {"test": 123};
	map.test = 1234;
	assert(map.test == 1234);
}
{
	var map = {"test": 123};
	const tmp = map.test;
	assert(tmp == 123);
	map.tmp = map.test;
	assert(map.tmp == 123);
}

assert(balance() == 0);
assert(balance(sha256("test")) == 0);
assert(this.balance[bech32()] == 0);
{
	var tmp = balance();
	assert(tmp == 0);
	tmp = 1337;
	assert(balance() == 0);
}
{
	var tmp = this.balance[bech32()];
	assert(tmp == 0);
	tmp = 1337;
	assert(balance(bech32()) == 0);
}

{
	var array = [1, 2, 3];
	for(var entry of array) {
		entry = 0;
	}
	for(const entry of array) {
		assert(entry);
	}
}
{
	var array = ["1", "2", "3"];
	for(var entry of array) {
		entry = "0";
	}
	for(const entry of array) {
		assert(entry != "0");
	}
}
{
	var array = [[1], [2], [3]];
	for(var entry of array) {
		entry = [];
	}
	for(const entry of array) {
		assert(size(entry));
	}
}
{
	var array = [{val: 1}, {val: 2}, {val: 3}];
	for(var entry of array) {
		entry = {val: 0};
	}
	for(const entry of array) {
		assert(entry.val);
	}
}

{
	var obj = {key: "value"};
	assert(obj.key == "value");
}
{
	var obj = {"key": "value"};
	assert(obj.key == "value");
}
{
	var obj = {test: 123, key: "value"};
	assert(obj.key == "value");
}
{
	var obj = { key : "value" };
	assert(obj.key == "value");
}
{
	var obj = { 	key		 : 	"value" };
	assert(obj.key == "value");
}
{
	var obj = {_key_: "value"};
	assert(obj._key_ == "value");
}

assert(1337 / 16 == 83);
assert(1337 % 16 == 9);
assert(1337133713371337 / 1024 == 1305794641964);
assert(1337133713371337 % 1024 == 201);

if(false) {
	fail("if(false)");
}
if(!true) {
	fail("if(!true)");
}
if(1 == 2) {
	fail("if(1 == 2)");
}
if(1 != 1) {
	fail("if(1 != 1)");
}
if(2 < 1) {
	fail("if(2 < 1)");
}
if(1 > 2) {
	fail("if(1 > 2)");
}
if(2 <= 1) {
	fail("if(2 <= 1)");
}
if(1 >= 2) {
	fail("if(1 >= 2)");
}
if(!(1 == 1)) {
	fail("if(!(1 == 1))");
}
if((1 == 1) && (1 == 0)) {
	fail("if((1 == 1) && (1 == 0))");
}
if((1 != 1) || (1 != 1)) {
	fail("if((1 != 1) || (1 != 1))");
}
if(0) {
	fail("if(0)");
}
if(null) {
	fail("if(null)");
}
if("") {
	fail("if('')");
}
if(!1337) {
	fail("if(!1337)");
}
if(!"test") {
	fail("if(!'test')");
}
if(!(true && 1337 && "test")) {
	fail("if(!(1 && true && 'test'))");
}
if(0 || null || "") {
	fail("if(0 || null || '')");
}
if(!(0 || null || 1)) {
	fail("if(!(null || null || 1))");
}
{
	var cond = (1 > 2);
	if(cond) {
		fail("if(cond)");
	}
}
{
	const cond = (1 > 2);
	if(cond) {
		fail("if(cond)");
	}
}
{
	var tmp = 1;
	tmp += 1;
	assert(tmp == 2);
}
{
	var tmp = 1;
	tmp -= 1;
	assert(tmp == 0);
}
{
	var tmp = 10;
	tmp *= 2;
	assert(tmp == 20);
}
{
	var tmp = 10;
	tmp /= 2;
	assert(tmp == 5);
}
{
	var tmp = 0;
	tmp ^= 0xFF;
	assert(tmp == 0xFF);
}
{
	var tmp = 0xFF;
	tmp &= 0xFF;
	assert(tmp == 0xFF);
}
{
	var tmp = 0;
	tmp |= 0xFF;
	assert(tmp == 0xFF);
}
{
	var tmp = false;
	tmp ^^= true;
	assert(tmp == true);
}
{
	var tmp = false;
	tmp &&= true;
	assert(tmp == false);
}
{
	var tmp = false;
	tmp ||= true;
	assert(tmp == true);
}
{
	assert(is_uint(1337));
	assert(is_string("test"));
	assert(is_binary(bech32()));
	assert(is_array([]));
	assert(is_map({}));
}
{
	assert(!is_uint(null));
	assert(!is_uint(true));
	assert(!is_uint(false));
	assert(!is_uint("test"));
	assert(!is_string(null));
	assert(!is_string(true));
	assert(!is_string(false));
	assert(!is_string(1337));
	assert(!is_array(null));
	assert(!is_map(null));
}
{
	while(false) {
		fail("while(false)");
	}
}
{
	var i = 0;
	while(i < 10) {
		i++;
	}
	assert(i == 10);
}


