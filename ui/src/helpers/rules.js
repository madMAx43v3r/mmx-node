const rules = {
    // is empty or non-negative number
    number: (value) => !value || /^\d+$/.test(value) || "Invalid number", //TODO i18n

    // is empty or valid mmx address
    address: (value) => !value || validateAddress(value) || "Invalid address", //TODO i18n

    // is non empty
    required: (value) => !isEmpty(value) || "Field is required",

    amount: (value) => {
        //if (value && value.length && value.match(/^(\d+([.,]\d*)?)$/)) {
        return isEmpty(value) || (typeof value === "number" && value > 0) || "Invalid amount";
    },

    memo: (value) => {
        if (value && value.length > 64) {
            return "Maximum length is 64";
        }
        return true;
    },
};

export default rules;
