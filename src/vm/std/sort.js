
function sort(arr) {
    // bubble sort ASC
    const N = size(arr);
    for(var i = 0; i < N - 1; i++) {
        for(var j = 0; j < N - i - 1; j++) {
            if(arr[j] > arr[j + 1]) {
                const tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
    return arr;
}

function reverse(arr) {
    const N = size(arr);
    const out = [];
    for(var i = 0; i < N; i++) {
        out.push(arr[N - i - 1]);
    }
    return out;
}

// Example usage
const array = [5, 2, 9, 1, 5, 6];
console.log(sort(array)); // Output: [1, 2, 5, 5, 6, 9]
