<script lang="ts">
    export let url: string | undefined;
    
    let urls = [{
        title: 'Dashboard',
        href: '/',
        active: true
    }];

    function update(_url: any) {
        let len = 0;

        let copy = urls.slice();
        copy.sort((a, b) => b.href.length - a.href.length);

        for (let item of copy) {
            if (url?.toLowerCase().startsWith(item.href) &&
                item.href.length >= len) {
                len = item.href.length;
                item.active = true;
            } else {
                item.active = false;
            }
        }

        urls = urls;
    }

    $: update(url);
</script>

<nav>
    {#each urls as url}
        <a class:active={url.active} href={url.href}>{url.title}</a>
    {/each}
</nav>

<style lang="scss">
    nav {
        background: #102822;
        padding: 15pt;
        margin-bottom: 15pt;
        display: flex;
        gap: 15pt;

        & > a {
            display: inline-block;
            padding: 5pt;
            color: #d1e5d9;
            text-decoration: none;
            border-bottom: 3px solid rgba(209, 229, 217, 0.30);
            font-family: Overpass, sans-serif;

            &.active,
            &:hover {
                border-bottom: 3px solid #d1e5d9;
            }
        }
    }
</style>