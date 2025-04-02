
function equals(L, R) {
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
