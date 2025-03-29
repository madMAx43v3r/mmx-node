import { useSessionMutation } from "@/queries/server";
import { usePeerInfoMutation } from "@/queries/api";
import { useNodeInfoMutation } from "@/queries/wapi";

export const NodeStatuses = Object.freeze({
    DisconnectedFromNode: Symbol("DisconnectedFromNode"),
    QueryTakingLong: Symbol("QueryTakingLong"),
    //LoggedOff: Symbol("LoggedOff"),
    Connecting: Symbol("Connecting"),
    Syncing: Symbol("Syncing"),
    Synced: Symbol("Synced"),
    None: Symbol("None"),
});

export const useNodeStatus = () => {
    const sessionStore = useSessionStore();
    const { isLocalNode } = useConfigData();

    const sessionFails = ref(0);
    const peerFails = ref(0);
    const syncFails = ref(1);

    const isQueryTakingLong = useIsQueryTakingLong(1000);
    const connectedToNode = computed(() => sessionFails.value < 1);
    const connectedToNetwork = computed(() => peerFails.value < 1 || !isLocalNode.value);
    const synced = computed(() => syncFails.value < 1);

    // --- Session
    const session = useSessionMutation();
    const sessionIntervalFn = () => {
        if (sessionStore.isLoggedIn) {
            session.mutate(null, {
                onSuccess: (data) => {
                    if (data && data.user) {
                        sessionFails.value = 0;
                    } else {
                        throw new Error("No valid session");
                    }
                },
                onError: () => {
                    sessionFails.value++;
                },
            });
        }
    };

    const sessionInterval = computed(() => {
        let interval = connectedToNode.value && connectedToNetwork.value ? 5000 : 1000;
        if (!sessionStore.isLoggedIn) {
            interval = -1;
        }
        return interval;
    });

    useIntervalFn2(sessionIntervalFn, sessionInterval);

    // --- Peers
    const peerInfo = usePeerInfoMutation();
    const peerInfoIntervalFn = () =>
        peerInfo.mutate(null, {
            onSuccess: (data) => {
                if (data.peers?.length > 0) {
                    peerFails.value = 0;
                } else {
                    throw new Error("No peers");
                }
            },
            onError: () => {
                peerFails.value++;
            },
        });

    const syncing = ref(false);
    const peerInfoInterval = computed(() => {
        let interval = connectedToNetwork.value ? 5000 : 1000;

        if (!connectedToNode.value || syncing.value) {
            interval = -1;
        }
        return interval;
    });

    useIntervalFn2(peerInfoIntervalFn, peerInfoInterval);

    // --- Sync
    const nodeInfoMutation = useNodeInfoMutation();
    const nodeInfoIntervalFn = () =>
        nodeInfoMutation.mutate(null, {
            onSuccess: (data) => {
                if (data) {
                    syncing.value = true;
                    if (data.is_synced) {
                        syncFails.value = 0;
                    }
                } else {
                    throw new Error("No Sync");
                }
            },
            onError: () => {
                syncing.value = false;
                syncFails.value++;
            },
        });

    const nodeInfoInterval = computed(() => {
        let interval = connectedToNetwork.value ? 5000 : 1000;
        if (!connectedToNode.value) {
            interval = -1;
        }
        return interval;
    });
    useIntervalFn2(nodeInfoIntervalFn, nodeInfoInterval);

    // --- currentStatus
    const currentStatus = computed(() => {
        let status = NodeStatuses.None;

        if (connectedToNode.value) {
            status = NodeStatuses.Connecting;
            if (connectedToNetwork.value) {
                status = NodeStatuses.Syncing;
                if (synced.value) {
                    status = NodeStatuses.Synced;
                }
            }

            // if (isQueryTakingLong.value) {
            //     status = NodeStatuses.QueryTakingLong;
            // }
        } else {
            status = NodeStatuses.DisconnectedFromNode;
        }

        return status;
    });

    // eslint-disable-next-line no-undef
    if (process.env.NODE_ENV !== "production") {
        // --- Debug
        watchEffect(() => {
            console.debug("isQueryTakingLong", isQueryTakingLong.value);
            //console.debug("sessionFails", sessionFails.value);
            console.debug("connectedToNode", connectedToNode.value);
            //console.debug("peerFails", peerFails.value);
            console.debug("connectedToNetwork", connectedToNetwork.value);
            //console.debug("syncFails", syncFails.value);
            console.debug("synced", synced.value);
            console.debug("currentStatus", currentStatus.value);
        });
    }

    return {
        status: currentStatus,
    };
};

import { useIntervalFn } from "@vueuse/core";
const useIntervalFn2 = (cb, interval) => {
    cb();

    const intervalFn = useIntervalFn(cb, interval);

    watch(interval, (newInterval, oldInterval) => {
        // Pause the interval function if the new interval is zero or negative
        if (toValue(newInterval) <= 0) {
            intervalFn.pause();
        }
        // If transitioning from a non-positive to positive interval, or if the interval decreases, invoke the callback
        if (
            (toValue(oldInterval) <= 0 && toValue(newInterval) > 0) ||
            (toValue(oldInterval) > 0 && toValue(newInterval) < toValue(oldInterval))
        ) {
            intervalFn.resume();
            cb();
        }
    });

    return intervalFn;
};
