#include <map>
#include <string>

namespace mmx {
namespace vm {

std::map<std::string, std::string> std_file_map = {
    {"equals.js", "\nfunction equals(L, R) {\n    if(L == R) {\n        return true;\n    }\n    if(!is_array(L) || !is_array(R)) {\n        return L == R;\n    }\n    const N = size(L);\n    if(size(R) != N) {\n        return false;\n    }\n    for(var i = 0; i < N; ++i) {\n        if(!equals(L[i], R[i])) {\n            return false;\n        }\n    }\n    return true;\n}"},
    {"reverse.js", "\nfunction reverse(arr) {\n    const N = size(arr);\n    const out = [];\n    for(var i = 0; i < N; i++) {\n        push(out, arr[N - i - 1]);\n    }\n    return out;\n}"},
    {"sort.js", "\nfunction sort(arr) {\n    // bubble sort ASC\n    const N = size(arr);\n    if(N <= 1) {\n        return arr;\n    }\n    const N_1 = N - 1;\n    for(var i = 0; i < N_1; i++) {\n        const M = N_1 - i;\n        for(var j = 0; j < M; j++) {\n            const k = j + 1;\n            const L = arr[j];\n            const R = arr[k];\n            if(L > R) {\n                arr[j] = R;\n                arr[k] = L;\n            }\n        }\n    }\n    return arr;\n}"},
};

} // namespace vm
} // namespace mmx
