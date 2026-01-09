// Same as == operator, but does a deep (recursive) comparison for arrays
// @returns true if the two values are equal, false otherwise
// Note: Objects cannot be compared, as such comparison is based on the reference (address)
function equals(L, R) const {
    if(L == R) {
        return true;
    }
    if(!is_array(L) || !is_array(R)) {
        return L == R;
    }
    const N = size(L);
    if(size(R) != N) {
        return false;
    }
    for(var i = 0; i < N; ++i) {
        if(!equals(L[i], R[i])) {
            return false;
        }
    }
    return true;
}
