
Vue.component('seed-autocomplete', {
    props: {
        value: String,
        word_list: Array,
        index: Number,
        word_count: Number,
        disabled: Boolean
	},
	model: {
		prop: 'value',
		event: 'input'
	},
    data() {
        return {
            items: [],
            queryTerm: null,
            word_list: []
        }
    },
    watch: {
        word_list() {
            this.queryTerm = ''
            this.items = this.word_list
        },
    },
    computed: {
        search: {
            get () {
                return this.queryTerm
            },        
            set (searchInput) {
                if (this.queryTerm !== searchInput) {
                    this.queryTerm = (searchInput || '').trim()
                    if(this.queryTerm) {
                        this.items = this.word_list.filter(i => i.indexOf(this.queryTerm) == 0)
                    } else {
                        this.items = this.word_list
                    }

                    if(this.items[0] == this.queryTerm) {
                        this.value = this.items[0]
                        this.$emit('input', this.value);
                    }

                    //console.log(this.value, this.queryTerm, this.items)
                }
            }
        },        
    },
    methods: {
        customFilter (item, queryText, itemText) {
            const text = item.toLowerCase().trim()
            const searchText = queryText.toLowerCase().trim()
            return text.indexOf(searchText) == 0
        },        
        onInput(event) {
            this.$emit('input', event);
        },
        onPaste(event) {
            
            this.$emit('paste', event)

            const text = event.clipboardData.getData('text');
            var wordArray = text.split(' ');            
            if(wordArray.length == this.word_count) {
                event.preventDefault();
                this.value = wordArray[this.index]
                this.queryTerm = wordArray[this.index]
                this.items = this.word_list.filter(i => i.indexOf(this.queryTerm) == 0)
            }            
        }
    },    
    template: 
        `
        <v-autocomplete
            v-model="value"
            :items="items"
            :label="index+1"
            :filter="customFilter"
            :search-input.sync="search"
            hide-details
            @input="onInput"
            @paste="onPaste"
            :disabled="disabled"
            outlined
            append-icon=""/>   
        `
});    

Vue.component('seed', {
    props: {
        value: String,
        readonly: Boolean,
        disabled: Boolean
	},
	model: {
		prop: 'value',
		event: 'input'
	},
    data() {
        return {
            words: (this.value || '').split(' '),
            word_count: 24,
            rows: 4,
            cols: 6,
            word_list: []
        }
    },
	created() {
        fetch('/api/wallet/get_mnemonic_wordlist')
            .then(response => response.json())
            .then(data => {
                this.word_list = data;
            });
	},

    watch: {
        value() {
            this.words = (this.value || '').split(' ')
        },
    },

    methods: {
        onPaste(event) {
            const text = event.clipboardData.getData('text');
            var wordArray = text.split(' ');
            if(wordArray.length == this.word_count) {
                this.words = wordArray
            }
        },
        onInput() {
            this.$emit('input', this.words.join(' '))
        }
    },    

	template: `
        <div>
            <v-row no-gutters v-for="row in [...Array(rows).keys()]">

                <v-col
                    v-for="col in [...Array(cols).keys()]"
                    cols="12"
                    sm="2"                                    
                >
                    <div v-if="readonly">
                        <v-text-field 
                            :label="row * cols + col + 1"
                            :value="words[row * cols + col]"
                            hide-details
                            outlined
                            readonly
                            class="ma-1" />
                    </div>

                    <div v-else> 
                        <seed-autocomplete
                            v-model="words[row * cols + col]"
                            :word_list="word_list"
                            :word_count="word_count"
                            :index="row * cols + col"
                            @paste="onPaste"
                            @input="onInput"
                            :disabled="disabled"
                            class="ma-1" />
                    </div>

                </v-col>
            
            </v-row>

        </div>
    `    
})