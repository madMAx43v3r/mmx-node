interface __test;
interface storage_map;

const binary = __test.compile("test/vm/storage_map_contract.js");

storage_map.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: [{dot: 11, bracket: 22}]
});

function assert_config(dot, bracket)
{
    const config = storage_map.get_config();
    assert(config.dot == dot);
    assert(config.bracket == bracket);
    assert(config.sum == dot + bracket);
}

function main()
{
    assert_config(11, 22);

    storage_map.update({dot: 33, bracket: 44});
    assert_config(33, 44);
}

main();
