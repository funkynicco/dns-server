<h1><slot></slot></h1>

<style lang="scss">
    @font-face {
        font-family: 'nordlo';
        src: url('/fonts/nordlo.woff') format('woff');
    }
    
    h1 {
        font-size: 70px;
        line-height: 1.12;
        font-family: 'nordlo', sans-serif;
        font-weight: 900;
        padding: 0;
        margin: 0;
        color: #f3dbc6;
        overflow-wrap: break-word;
        text-shadow: 0 0 2px rgba(0, 0, 0, 1);
    }
</style>