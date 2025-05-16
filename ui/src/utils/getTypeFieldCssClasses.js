export const getTypeFieldCssClasses = (type) => {
    if (type == "REWARD") return "text-lime-8";
    if (type == "RECEIVE" || type == "REWARD" || type == "VDF_REWARD") return "text-positive";
    if (type == "SPEND") return "text-negative";
    if (type == "TXFEE") return "text-grey";
    return "";
};
