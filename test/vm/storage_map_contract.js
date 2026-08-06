var config = {};

function init(input)
{
    assert(is_map(input), "invalid config");

    config.dot = uint(input.dot);
    config["bracket"] = uint(input["bracket"]);
    config.sum = config.dot + config.bracket;
}

function update(input) public
{
    assert(is_map(input), "invalid config");

    config.dot = uint(input.dot);
    config["bracket"] = uint(input["bracket"]);
    config.sum = config.dot + config.bracket;
}

function get_config() const public
{
    return {
        dot: config.dot,
        bracket: config["bracket"],
        sum: config.sum,
    };
}
