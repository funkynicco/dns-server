<script lang="ts">
    import {fade, fly} from "svelte/transition";

    let is_open = false;
    
    export let nopadding = false;
    export let title = '';
    
    export function toggle() {
        is_open = !is_open;
    }
    
    export function open() {
        is_open = true;
    }
    
    export function close() {
        is_open = false;
    }
</script>

{#if is_open}
    <div class="backdrop" transition:fade={{duration: 100}}></div>
    <div class="popup-container" on:click={() => close()}>
        <div class="popup" class:nopadding on:click={ev => ev.stopPropagation()}  in:fly={{ y: 200, duration: 150, delay: 0 }} out:fly={{ y: -200, duration: 150 }}>
            {#if title}
                <h1>{title}</h1>
            {/if}
            <slot />
        </div>
    </div>
{/if}

<style lang="scss">
    .backdrop {
        position: fixed;
        left: 0;
        top: 0;
        right: 0;
        bottom: 0;
        background: rgba(0, 0, 0, 0.6);
        z-index: 1000;
    }
    
    .popup-container {
        position: fixed;
        left: 0;
        top: 0;
        right: 0;
        bottom: 0;
        z-index: 1001;        
    }
    
    .popup {
        width: var(--popup-width, 340px);
        min-height: 160px;
        margin: 20vh auto;
        background: #1d3c34;
        box-shadow: 0 0 5px #000;
        padding: 14pt;
        
        & > h1 {
            margin: 0;
        }
    }
    
    .popup.nopadding {
        padding: inherit;
    }
</style>