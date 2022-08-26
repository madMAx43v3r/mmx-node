
const eventsName = [
  "AfterExport",
  "AfterPlot",
  "Animated",
  "AnimatingFrame",
  "AnimationInterrupted",
  "AutoSize",
  "BeforeExport",
  "ButtonClicked",
  "Click",
  "ClickAnnotation",
  "Deselect",
  "DoubleClick",
  "Framework",
  "Hover",
  "LegendClick",
  "LegendDoubleClick",
  "Relayout",
  "Restyle",
  "Redraw",
  "Selected",
  "Selecting",
  "SliderChange",
  "SliderEnd",
  "SliderStart",
  "Transitioning",
  "TransitionInterrupted",
  "Unhover"
];

const eventsP = eventsName
  .map(evt => evt.toLocaleLowerCase())
  .map(eventName => ({
    completeName: "plotly_" + eventName,
    handler: context => (...args) => {
      context.$emit.apply(context, [eventName, ...args]);
    }
  }));

const plotlyFunctions = ["restyle", "relayout", "update", "addTraces", "deleteTraces", "moveTraces", "extendTraces", "prependTraces", "purge"];

const methods = plotlyFunctions.reduce((all, functionName) => {
  all[functionName] = function(...args) {
    return Plotly[functionName].apply(Plotly, [this.$el, ...args]);
  };
  return all;
}, {});

function cached(fn) {
  const cache = Object.create(null);
  return function cachedFn(str) {
    const hit = cache[str];
    return hit || (cache[str] = fn(str));
  };
}

const regex = /-(\w)/g;
const camelizeP = cached(str => str.replace(regex, (_, c) => (c ? c.toUpperCase() : "")));

const darkTemplate = {
  "data": {
      "barpolar": [
          {
              "marker": {
                  "line": {
                      "color": "rgb(30,30,30)",
                      "width": 0.5
                  },
                  "pattern": {
                      "fillmode": "overlay",
                      "size": 10,
                      "solidity": 0.2
                  }
              },
              "type": "barpolar"
          }
      ],
      "bar": [
          {
              "error_x": {
                  "color": "#f2f5fa"
              },
              "error_y": {
                  "color": "#f2f5fa"
              },
              "marker": {
                  "line": {
                      "color": "rgb(30,30,30)",
                      "width": 0.5
                  },
                  "pattern": {
                      "fillmode": "overlay",
                      "size": 10,
                      "solidity": 0.2
                  }
              },
              "type": "bar"
          }
      ],
      "carpet": [
          {
              "aaxis": {
                  "endlinecolor": "#A2B1C6",
                  "gridcolor": "#506784",
                  "linecolor": "#506784",
                  "minorgridcolor": "#506784",
                  "startlinecolor": "#A2B1C6"
              },
              "baxis": {
                  "endlinecolor": "#A2B1C6",
                  "gridcolor": "#506784",
                  "linecolor": "#506784",
                  "minorgridcolor": "#506784",
                  "startlinecolor": "#A2B1C6"
              },
              "type": "carpet"
          }
      ],
      "choropleth": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "type": "choropleth"
          }
      ],
      "contourcarpet": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "type": "contourcarpet"
          }
      ],
      "contour": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "contour"
          }
      ],
      "heatmapgl": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "heatmapgl"
          }
      ],
      "heatmap": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "heatmap"
          }
      ],
      "histogram2dcontour": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "histogram2dcontour"
          }
      ],
      "histogram2d": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "histogram2d"
          }
      ],
      "histogram": [
          {
              "marker": {
                  "pattern": {
                      "fillmode": "overlay",
                      "size": 10,
                      "solidity": 0.2
                  }
              },
              "type": "histogram"
          }
      ],
      "mesh3d": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "type": "mesh3d"
          }
      ],
      "parcoords": [
          {
              "line": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "parcoords"
          }
      ],
      "pie": [
          {
              "automargin": true,
              "type": "pie"
          }
      ],
      "scatter3d": [
          {
              "line": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scatter3d"
          }
      ],
      "scattercarpet": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scattercarpet"
          }
      ],
      "scattergeo": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scattergeo"
          }
      ],
      "scattergl": [
          {
              "marker": {
                  "line": {
                      "color": "#283442"
                  }
              },
              "type": "scattergl"
          }
      ],
      "scattermapbox": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scattermapbox"
          }
      ],
      "scatterpolargl": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scatterpolargl"
          }
      ],
      "scatterpolar": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scatterpolar"
          }
      ],
      "scatter": [
          {
              "marker": {
                  "line": {
                      "color": "#283442"
                  }
              },
              "type": "scatter"
          }
      ],
      "scatterternary": [
          {
              "marker": {
                  "colorbar": {
                      "outlinewidth": 0,
                      "ticks": ""
                  }
              },
              "type": "scatterternary"
          }
      ],
      "surface": [
          {
              "colorbar": {
                  "outlinewidth": 0,
                  "ticks": ""
              },
              "colorscale": [
                  [
                      0.0,
                      "#0d0887"
                  ],
                  [
                      0.1111111111111111,
                      "#46039f"
                  ],
                  [
                      0.2222222222222222,
                      "#7201a8"
                  ],
                  [
                      0.3333333333333333,
                      "#9c179e"
                  ],
                  [
                      0.4444444444444444,
                      "#bd3786"
                  ],
                  [
                      0.5555555555555556,
                      "#d8576b"
                  ],
                  [
                      0.6666666666666666,
                      "#ed7953"
                  ],
                  [
                      0.7777777777777778,
                      "#fb9f3a"
                  ],
                  [
                      0.8888888888888888,
                      "#fdca26"
                  ],
                  [
                      1.0,
                      "#f0f921"
                  ]
              ],
              "type": "surface"
          }
      ],
      "table": [
          {
              "cells": {
                  "fill": {
                      "color": "#506784"
                  },
                  "line": {
                      "color": "rgb(30,30,30)"
                  }
              },
              "header": {
                  "fill": {
                      "color": "#2a3f5f"
                  },
                  "line": {
                      "color": "rgb(30,30,30)"
                  }
              },
              "type": "table"
          }
      ]
  },
  "layout": {
      "annotationdefaults": {
          "arrowcolor": "#f2f5fa",
          "arrowhead": 0,
          "arrowwidth": 1
      },
      "autotypenumbers": "strict",
      "coloraxis": {
          "colorbar": {
              "outlinewidth": 0,
              "ticks": ""
          }
      },
      "colorscale": {
          "diverging": [
              [
                  0,
                  "#8e0152"
              ],
              [
                  0.1,
                  "#c51b7d"
              ],
              [
                  0.2,
                  "#de77ae"
              ],
              [
                  0.3,
                  "#f1b6da"
              ],
              [
                  0.4,
                  "#fde0ef"
              ],
              [
                  0.5,
                  "#f7f7f7"
              ],
              [
                  0.6,
                  "#e6f5d0"
              ],
              [
                  0.7,
                  "#b8e186"
              ],
              [
                  0.8,
                  "#7fbc41"
              ],
              [
                  0.9,
                  "#4d9221"
              ],
              [
                  1,
                  "#276419"
              ]
          ],
          "sequential": [
              [
                  0.0,
                  "#0d0887"
              ],
              [
                  0.1111111111111111,
                  "#46039f"
              ],
              [
                  0.2222222222222222,
                  "#7201a8"
              ],
              [
                  0.3333333333333333,
                  "#9c179e"
              ],
              [
                  0.4444444444444444,
                  "#bd3786"
              ],
              [
                  0.5555555555555556,
                  "#d8576b"
              ],
              [
                  0.6666666666666666,
                  "#ed7953"
              ],
              [
                  0.7777777777777778,
                  "#fb9f3a"
              ],
              [
                  0.8888888888888888,
                  "#fdca26"
              ],
              [
                  1.0,
                  "#f0f921"
              ]
          ],
          "sequentialminus": [
              [
                  0.0,
                  "#0d0887"
              ],
              [
                  0.1111111111111111,
                  "#46039f"
              ],
              [
                  0.2222222222222222,
                  "#7201a8"
              ],
              [
                  0.3333333333333333,
                  "#9c179e"
              ],
              [
                  0.4444444444444444,
                  "#bd3786"
              ],
              [
                  0.5555555555555556,
                  "#d8576b"
              ],
              [
                  0.6666666666666666,
                  "#ed7953"
              ],
              [
                  0.7777777777777778,
                  "#fb9f3a"
              ],
              [
                  0.8888888888888888,
                  "#fdca26"
              ],
              [
                  1.0,
                  "#f0f921"
              ]
          ]
      },
      "colorway": [
          "#636efa",
          "#EF553B",
          "#00cc96",
          "#ab63fa",
          "#FFA15A",
          "#19d3f3",
          "#FF6692",
          "#B6E880",
          "#FF97FF",
          "#FECB52"
      ],
      "font": {
          "color": "#f2f5fa"
      },
      "geo": {
          "bgcolor": "rgb(30,30,30)",
          "lakecolor": "rgb(30,30,30)",
          "landcolor": "rgb(30,30,30)",
          "showlakes": true,
          "showland": true,
          "subunitcolor": "#506784"
      },
      "hoverlabel": {
          "align": "left"
      },
      "hovermode": "closest",
      "mapbox": {
          "style": "dark"
      },
      "paper_bgcolor": "rgb(30,30,30)",
      "plot_bgcolor": "rgb(30,30,30)",
      "polar": {
          "angularaxis": {
              "gridcolor": "#506784",
              "linecolor": "#506784",
              "ticks": ""
          },
          "bgcolor": "rgb(30,30,30)",
          "radialaxis": {
              "gridcolor": "#506784",
              "linecolor": "#506784",
              "ticks": ""
          }
      },
      "scene": {
          "xaxis": {
              "backgroundcolor": "rgb(30,30,30)",
              "gridcolor": "#506784",
              "gridwidth": 2,
              "linecolor": "#506784",
              "showbackground": true,
              "ticks": "",
              "zerolinecolor": "#C8D4E3"
          },
          "yaxis": {
              "backgroundcolor": "rgb(30,30,30)",
              "gridcolor": "#506784",
              "gridwidth": 2,
              "linecolor": "#506784",
              "showbackground": true,
              "ticks": "",
              "zerolinecolor": "#C8D4E3"
          },
          "zaxis": {
              "backgroundcolor": "rgb(30,30,30)",
              "gridcolor": "#506784",
              "gridwidth": 2,
              "linecolor": "#506784",
              "showbackground": true,
              "ticks": "",
              "zerolinecolor": "#C8D4E3"
          }
      },
      "shapedefaults": {
          "line": {
              "color": "#f2f5fa"
          }
      },
      "sliderdefaults": {
          "bgcolor": "#C8D4E3",
          "bordercolor": "rgb(30,30,30)",
          "borderwidth": 1,
          "tickwidth": 0
      },
      "ternary": {
          "aaxis": {
              "gridcolor": "#506784",
              "linecolor": "#506784",
              "ticks": ""
          },
          "baxis": {
              "gridcolor": "#506784",
              "linecolor": "#506784",
              "ticks": ""
          },
          "bgcolor": "rgb(30,30,30)",
          "caxis": {
              "gridcolor": "#506784",
              "linecolor": "#506784",
              "ticks": ""
          }
      },

      "updatemenudefaults": {
          "bgcolor": "#506784",
          "borderwidth": 0
      },
      "xaxis": {
          "automargin": true,
          "gridcolor": "#283442",
          "linecolor": "#506784",
          "ticks": "",
          "title": {
              "standoff": 15
          },
          "zerolinecolor": "#283442",
          "zerolinewidth": 2
      },
      "yaxis": {
          "automargin": true,
          "gridcolor": "#283442",
          "linecolor": "#506784",
          "ticks": "",
          "title": {
              "standoff": 15
          },
          "zerolinecolor": "#283442",
          "zerolinewidth": 2
      }
  }
}

Vue.component('vue-plotly',
{
  inheritAttrs: false,
  props: {
    data: {
      type: Array
    },
    layout: {
      type: Object
    },
    id: {
      type: String,
      required: false,
      default: null
    }
  },
  data() {
    return {
      scheduled: null,
      innerLayout: { ...this.layout }
    };
  },
  mounted() {
    this.innerLayout = { ...this.layout, template: this.template };
    Plotly.newPlot(this.$el, this.data, this.innerLayout, this.options);
    eventsP.forEach(evt => {
      this.$el.on(evt.completeName, evt.handler(this));
    });
  },
  watch: {
    data: {
      handler() {
        this.schedule({ replot: true });
      },
      deep: true
    },
    options: {
      handler(value, old) {
        if (JSON.stringify(value) === JSON.stringify(old)) {
          return;
        }
        this.schedule({ replot: true });
      },
      deep: true
    },
    layout(layout) {
      this.innerLayout = { ...layout, template: this.template };
      this.schedule({ replot: false });
    }
  },
  computed: {
    template() {
      return this.$vuetify.theme.dark? darkTemplate : null;
    },
    options() {
      const optionsFromAttrs = Object.keys(this.$attrs).reduce((acc, key) => {
        acc[camelizeP(key)] = this.$attrs[key];
        return acc;
      }, {});
      return {
        responsive: false,
        ...optionsFromAttrs
      };
    }
  },
  beforeDestroy() {
    eventsP.forEach(event => this.$el.removeAllListeners(event.completeName));
    Plotly.purge(this.$el);
  },
  methods: {
    ...methods,
    onResize() {
      Plotly.Plots.resize(this.$el);
    },
    schedule(context) {
      const { scheduled } = this;
      if (scheduled) {
        scheduled.replot = scheduled.replot || context.replot;
        return;
      }
      this.scheduled = context;
      this.$nextTick(() => {
        const {
          scheduled: { replot }
        } = this;
        this.scheduled = null;
        if (replot) {
          this.react();
          return;
        }
        this.relayout(this.innerLayout);
      });
    },
    toImage(options) {
      const allOptions = Object.assign(this.getPrintOptions(), options);
      return Plotly.toImage(this.$el, allOptions);
    },
    downloadImage(options) {
      const filename = `plot--${new Date().toISOString()}`;
      const allOptions = Object.assign(this.getPrintOptions(), { filename }, options);
      return Plotly.downloadImage(this.$el, allOptions);
    },
    getPrintOptions() {
      const { $el } = this;
      return {
        format: "png",
        width: $el.clientWidth,
        height: $el.clientHeight
      };
    },
    react() {
      Plotly.react(this.$el, this.data, this.innerLayout, this.options);
    }
  },
  template: `
    <div :id="id"></div>
  `
})
