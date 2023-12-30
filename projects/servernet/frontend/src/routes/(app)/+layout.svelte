<script lang="ts">
    import {fly} from "svelte/transition";

    export let data;
</script>

<div class="afds">
{#key data.url}
    <div in:fly={{ x: -200, duration: 150, delay: 150 }}
         out:fly={{ x: 200, duration: 150 }}>
        <slot></slot>
    </div>
{/key}
</div>

<style lang="scss">
    @import "$lib/styles/Palette.scss";
    
    :global(body) {
        background: $c1;
        color: $c3;
    }
    
    .afds {
        background: $c2;
    }
</style>