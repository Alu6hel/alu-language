module.exports = {
  title: 'ALU Language Documentation',
  tagline: 'High-performance image processing engine',
  url: 'https://alu6hel.github.io',
  baseUrl: '/',
  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',
  favicon: 'img/favicon.ico',
  organizationName: 'Alu6hel',
  projectName: 'alu-language',
  themeConfig: {
    navbar: {
      title: 'ALU Docs',
      items: [
        {to: 'docs/quickstart', label: '10-Minute Quickstart', position: 'left'},
        {to: 'docs/api_reference', label: 'API Reference', position: 'left'},
        {to: 'docs/language_spec', label: 'Language Spec', position: 'left'},
      ],
    },
    footer: {
      style: 'dark',
      copyright: `Copyright © ${new Date().getFullYear()} Alu Project. Built with Docusaurus.`,
    },
  },
  presets: [
    [
      '@docusaurus/preset-classic',
      {
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
        },
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      },
    ],
  ],
};
