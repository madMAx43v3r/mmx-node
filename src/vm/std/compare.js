// Returns "EQ" if arguments are equal, "LT" if L < R and "GT" if L > R
// Note: Arrays are (recursively) deep compared, otherwise the same as language operators
function compare(L, R) const {
    if(!is_array(L) || !is_array(R)) {
        if(L < R) {
            return "LT";
        }
        if(L > R) {
            return "GT";
        }
        return "EQ";
    }
    const N = size(L);
    const M = size(R);
    if(N != M) {
        if(N < M) {
            return "LT";
        }
        return "GT";
    }
    for(var i = 0; i < N; ++i) {
        const res = compare(L[i], R[i]);
        if(res != "EQ") {
            return res;
        }
    }
    return "EQ";
}
