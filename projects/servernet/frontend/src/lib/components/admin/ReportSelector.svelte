<script lang="ts">
    import {slide} from 'svelte/transition';

    export let type = 'purchase';

    let popup: HTMLDivElement;
    let is_open = false;

    const types = {
        purchase: 'Köp',
        report: 'Personrapport'
    };
    
    const types_list = [{
        'key': 'purchase',
        'name': 'Köp'
    }, {
        'key': 'report',
        'name': 'Personrapport'
    }];

    function toggle() {
        is_open = !is_open;
        if (is_open) {
            setTimeout(() => popup.focus(), 1);
        }
    }

    function close() {
        is_open = false;
    }
    
    function pick(key: string) {
        type = key;
        close();
    }
</script>

<div class="report-selector">
    <div class="selector" on:click={() => toggle()}>{types[type]} <i class="fa {is_open ? 'fa-caret-up' : 'fa-caret-down'}"></i></div>
    {#if is_open}
        <div bind:this={popup} transition:slide class="popup" tabindex="0" on:blur={() => close()}>
            <div class="popup-types">
                {#each types_list as this_type, i}
                    <div class:active={this_type.key === type} on:click={() => pick(this_type.key)}>{this_type.name}</div>
                {/each}
            </div>
        </div>
    {/if}
</div>

<style lang="scss">
    .report-selector {
        display: inline-block;
        
        & > .selector {
            display: inline-block;
            background: #d1e5d9;
            color: #000;
            padding: 7pt;
            font-size: 1.3em;
            font-family: Overpass, sans-serif;
            position: relative;

            &:hover {
                background: #e3efe7;
                cursor: pointer;
            }
        }

        & > .popup {
            z-index: 1000;
            background: #d1e5d9;
            position: absolute;
            font-size: 1.3em;
            color: #000;
            padding: 7pt;
            width: 300px;
            outline: none;

            & > .popup-types {
                overflow: hidden;

                & > div {
                    text-align: center;
                    background: #a7bbb0;
                    font-family: Overpass, sans-serif;
                    padding: 7pt;
                    border-radius: 7pt;
                    border: 3px solid rgba(0, 0, 0, 0);

                    &:not(:first-child) {
                        margin-top: 7pt;
                    }
                    
                    &:hover {
                        background: #8eb6a0;
                        cursor: pointer;
                    }

                    &.active {
                        border: 3px solid #000;
                    }
                }
            }
        }
    }
</style>