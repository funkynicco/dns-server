using DnsServer.Utilities;

namespace DnsServer.Tests;

public class WildcompareTests
{
    [Theory]
    [InlineData("", "")]
    [InlineData("a", "a")]
    [InlineData("ab", "ab")]
    [InlineData("ab", "a?")]
    [InlineData("ab", "?b")]
    [InlineData("abc", "a?c")]
    [InlineData("abc", "??c")]
    [InlineData("abc", "a*")]
    [InlineData("abc", "a*c")]
    [InlineData("abc", "ab*")]
    [InlineData("abc", "*ab*")]
    [InlineData("abc", "*b*")]
    [InlineData("abc", "*bc")]
    [InlineData("abc", "*b?")]
    [InlineData("abc", "**?")]
    public void ShouldBeSame(string str, string wildcard)
    {
        Assert.True(str.CompareWildcard(wildcard));
    }
    
    [Theory]
    [InlineData("", "a")]
    [InlineData("a", "b")]
    [InlineData("ab", "ba")]
    [InlineData("ab", "b?")]
    [InlineData("ab", "?c")]
    [InlineData("abc", "c?a")]
    [InlineData("abc", "??a")]
    [InlineData("abc", "b*")]
    [InlineData("abc", "e*c")]
    [InlineData("abc", "ae*")]
    [InlineData("abc", "*bb*")]
    [InlineData("abc", "*e*")]
    [InlineData("abc", "*qc")]
    [InlineData("abc", "*h?")]
    public void ShouldNotBeSame(string str, string wildcard)
    {
        Assert.False(str.CompareWildcard(wildcard));
    }
}