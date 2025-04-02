#include <map>
#include <string>

namespace mmx {
namespace vm {

std::map<std::string, std::string> std_file_map = {
    {"compare.js", "// Returns \"EQ\" if arguments are equal, \"LT\" if L < R and \"GT\" if L > R\n// Note: Arrays are (recursively) deep compared, otherwise the same as language operators\nfunction compare(L, R) {\n    if(!is_array(L) || !is_array(R)) {\n        if(L < R) {\n            return \"LT\";\n        }\n        if(L > R) {\n            return \"GT\";\n        }\n        return \"EQ\";\n    }\n    const N = size(L);\n    const M = size(R);\n    if(N != M) {\n        if(N < M) {\n            return \"LT\";\n        }\n        return \"GT\";\n    }\n    for(var i = 0; i < N; ++i) {\n        const res = compare(L[i], R[i]);\n        if(res != \"EQ\") {\n            return res;\n        }\n    }\n    return \"EQ\";\n}"},
    {"equals.js", "// Same as == operator, but does a deep (recursive) comparison for arrays\n// @returns true if the two values are equal, false otherwise\n// Note: Objects cannot be compared, as such comparison is based on the reference (address)\nfunction equals(L, R) {\n    if(L == R) {\n        return true;\n    }\n    if(!is_array(L) || !is_array(R)) {\n        return L == R;\n    }\n    const N = size(L);\n    if(size(R) != N) {\n        return false;\n    }\n    for(var i = 0; i < N; ++i) {\n        if(!equals(L[i], R[i])) {\n            return false;\n        }\n    }\n    return true;\n}"},
    {"reverse.js", "// Returns a new array with the elements of the input array in reverse order\nfunction reverse(arr) {\n    const N = size(arr);\n    const out = [];\n    for(var i = 0; i < N; i++) {\n        push(out, arr[N - i - 1]);\n    }\n    return out;\n}"},
    {"sort.js", "// Returns a sorted array in ascending order (using bubble sort)\n// Note: use reverse(sort(...)) to get a descending order\nfunction sort(arr) {\n    const N = size(arr);\n    if(N <= 1) {\n        return arr;\n    }\n    const N_1 = N - 1;\n    for(var i = 0; i < N_1; i++) {\n        const M = N_1 - i;\n        for(var j = 0; j < M; j++) {\n            const k = j + 1;\n            const L = arr[j];\n            const R = arr[k];\n            if(L > R) {\n                arr[j] = R;\n                arr[k] = L;\n            }\n        }\n    }\n    return arr;\n}"},
};

} // namespace vm
} // namespace mmx
