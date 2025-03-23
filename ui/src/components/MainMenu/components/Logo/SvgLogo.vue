<!-- https://github.com/voidxno/mmx-assets/blob/master/logo/vector/logo_text_black_nodots.svg -->
<template>
    <svg
        xmlns="http://www.w3.org/2000/svg"
        xml:space="preserve"
        style="
            shape-rendering: geometricPrecision;
            text-rendering: geometricPrecision;
            image-rendering: optimizeQuality;
            fill-rule: evenodd;
            clip-rule: evenodd;
        "
        viewBox="0 0 1170.7 327.595"
    >
        <g id="Text_x0020_Black_x0020__x0028_SVG_x0029_">
            <path
                d="M19.578 80.542 0 327.595h95.292l6.752-85.204zM258.854 0c-27.766 0-53.07 16.901-65.333 40.975l-20.02 39.29L132.604 0H25.96L173.5 289.567l91.121-178.834-17.184 216.863h95.292L368.689 0zM454.949 80.542l-19.578 247.053h95.292l6.752-85.204zM694.225 0c-27.766 0-53.07 16.901-65.333 40.975l-20.02 39.29L567.975 0H461.33l147.54 289.566 91.121-178.834-17.184 216.863H778.1L804.06 0zM918.156 184.837l53.254 104.729-19.377 38.029h-106.47zM908.537 0h69.457l166.918 327.595h-106.643L890.298 37.187C879.835 16.653 888.003.001 908.537.001zM1098.11 142.76l-53.26-104.725L1064.23 0h106.47z"
                class="fil0"
            />
        </g>
    </svg>
</template>

<script setup>
const props = defineProps({
    color: {
        type: String,
        default: "#fff",
    },
});
</script>

<style scoped>
.fil0 {
    fill: v-bind(color);
}
</style>
