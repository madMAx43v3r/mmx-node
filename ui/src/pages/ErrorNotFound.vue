<template>
    <q-card class="fullscreen text-center q-pa-md flex flex-center">
        <div>
            <div class="error-code">404</div>
            <div class="error-message">Oops. Nothing here...</div>
            <q-btn class="q-mt-xl" outline unelevated to="/" label="Go Home" no-caps />
        </div>
    </q-card>
</template>

<style scoped>
.error-code {
    font-size: 5vw;
}

.error-message {
    font-size: 2.5vw;
    opacity: 0.4;
}

@media (max-width: 1200px) {
    .error-code {
        font-size: 10vw;
    }

    .error-message {
        font-size: 5vw;
    }
}
</style>
