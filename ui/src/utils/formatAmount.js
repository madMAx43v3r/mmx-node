const intlFormat = new Intl.NumberFormat(navigator.language, { minimumFractionDigits: 1, maximumFractionDigits: 12 });

export const formatAmount = (value) => {
    return intlFormat.format(value);
};
