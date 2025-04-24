<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <ExploreSearchBar />
            <q-card flat>
                <ExploreMenu />
                <TrRouterView :show="!!route.meta.indexPage" />
            </q-card>
            <TrRouterView :show="!route.meta.indexPage" />
        </div>
    </q-page>
</template>

<script setup>
import ExploreSearchBar from "./ExploreSearchBar";
import ExploreMenu from "./ExploreMenu";

const route = useRoute();
</script>
