export const cost_to_fee = (cost, feeRatio) => {
    const fee = (BigInt(cost) * BigInt(feeRatio)) / 1024n;
    return fee;
};
