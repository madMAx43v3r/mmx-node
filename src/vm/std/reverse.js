// Returns a new array with the elements of the input array in reverse order
function reverse(arr) {
    const N = size(arr);
    const out = [];
    for(var i = 0; i < N; i++) {
        push(out, arr[N - i - 1]);
    }
    return out;
}
