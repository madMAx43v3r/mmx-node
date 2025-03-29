export const jsonSerializer = {
    read: (value) => (value != null ? JSON.parse(value) : null),
    write: (value) => JSON.stringify(value),
};
