export const getXClasses = (bcProps) => {
    const xclasses = bcProps.col.xclasses;
    const value = bcProps.value;
    return xclasses instanceof Function ? xclasses(value) : xclasses;
};
