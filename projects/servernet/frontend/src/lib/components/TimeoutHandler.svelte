<script lang="ts">
    import {onMount} from "svelte";
    import {goto} from "$app/navigation";
    
    export let seconds = 300; // 5minutes
    
    export let target = '/';

    onMount(() => {
        const timer = setTimeout(async () => {
            await goto(target);
        }, seconds * 1000);
        
        return () => clearTimeout(timer);
    });
</script>
