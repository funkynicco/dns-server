<script lang="ts">
    import {slide} from 'svelte/transition';
    
    export let date = new Date();
    $: date_text = `${months[date.getMonth()]}, ${date.getFullYear()}`;
    
    let popup: HTMLDivElement;
    let is_open = false;
    
    const months = [
        'Januari',
        'Februari',
        'Mars',
        'April',
        'Maj',
        'Juni',
        'Juli',
        'Augusti',
        'September',
        'Oktober',
        'November',
        'December'
    ];
    
    function toggle() {
        is_open = !is_open;
        if (is_open) {
            setTimeout(() => popup.focus(), 0);
        }
    }
    
    function close() {
        is_open = false;
    }
    
    function pick(index) {
        date = new Date(date.getFullYear(), index, 1, 0, 0, 0);
        close();
    }
    
    function shiftYear(n: number) {
        date = new Date(date.getFullYear() + n, date.getMonth(), 1, 0, 0, 0);
    }
</script>

<div class="month-selector">
    <div class="selector" on:click={() => toggle()}>{date_text} <i class="fa {is_open ? 'fa-caret-up' : 'fa-caret-down'}"></i></div>
    {#if is_open}
        <div bind:this={popup} transition:slide class="popup" tabindex="0" on:blur={() => close()}>
            <div class="popup-year">
                <i class="fa fa-arrow-left" on:click={() => shiftYear(-1)}></i> <span>{date.getFullYear()}</span> <i class="fa fa-arrow-right" on:click={() => shiftYear(1)}></i>
            </div>
            <div class="popup-months">
                {#each months as month, i}
                    <div class:active={i === date.getMonth()} on:click={() => pick(i)}>{month.substring(0, 3)}</div>
                {/each}
            </div>
        </div>
    {/if}
</div>

<style lang="scss">
    .month-selector {
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
            $height: 300px;

            z-index: 1000;
            background: #d1e5d9;
            position: absolute;
            font-size: 1.3em;
            color: #000;
            padding: 7pt;
            width: 400px;
            height: $height;
            outline: none;

            & > .popup-year {
                text-align: center;
                margin: 10pt;
                
                & > i {
                    cursor: pointer;
                }
                
                & > span {
                    margin: 0 10pt 0 10pt;
                }
            }
            
            & > .popup-months {
                display: flex;
                overflow: hidden;
                flex-flow: wrap;
                gap: 10px;

                & > div {
                    text-align: center;
                    flex: 1 0 21%;
                    height: calc($height / 4.4);
                    background: #a7bbb0;
                    padding: 15pt 7pt 7pt 7pt;
                    border-radius: 7pt;
                    border: 3px solid rgba(0, 0, 0, 0);
                    font-family: Overpass, sans-serif;
                    
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