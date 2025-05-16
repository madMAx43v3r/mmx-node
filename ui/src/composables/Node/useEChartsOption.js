// --------------------------------------
// -- https://vue-echarts.dev/#codegen
import { use } from "echarts/core";
import { LineChart } from "echarts/charts";
import { TitleComponent, TooltipComponent, GridComponent } from "echarts/components";
import { CanvasRenderer } from "echarts/renderers";

use([TitleComponent, TooltipComponent, GridComponent, LineChart, CanvasRenderer]);
// --------------------------------------

// -- https://echarts.apache.org/en/option.html
export const useEChartsOption = () =>
    reactive({
        title: {
            //text: "TITLE",
            left: "center",
            top: "10px",
            textStyle: {
                color: "#1976D2",
            },
        },
        textStyle: {
            fontFamily: "Roboto Mono Variable",
            fontWeight: "normal",
        },
        tooltip: {
            trigger: "axis",
            axisPointer: {
                type: "line",
                label: {
                    precision: 0,
                },
            },
        },
        xAxis: {
            type: "value",
            min: (value) => value.min,
            max: (value) => value.max,
            axisLabel: {
                showMinLabel: false,
                showMaxLabel: false,
            },
            allowDecimals: false,
        },
        yAxis: {
            type: "value",
            axisLabel: {
                showMinLabel: false,
                showMaxLabel: false,
            },
            min: (value) => Math.max(Math.floor(value.min) - 1, 0),
            max: (value) => Math.ceil(value.max) + 1,
        },
        grid: {
            left: "10%",
            top: "10%",
            right: "10%",
            bottom: "10%",
        },
        series: [
            {
                type: "line",
                //data: unref(data),
                lineStyle: {
                    width: 2,
                },
                showSymbol: false,
            },
        ],
    });
