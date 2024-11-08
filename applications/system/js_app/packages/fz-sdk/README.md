# Momentum FW JavaScript SDK
This package contains tooling and typings for developing Flipper Zero
applications in JavaScript for Momentum Custom Firmware.

This is a fork of the [Official Flipper Zero JS SDK](https://www.npmjs.com/package/@flipperdevices/fz-sdk),
with added types for the extra features provided by the Momentum JavaScript API.

Scripts made for Official Flipper Zero JS SDK will work on Momentum Firmware too.
If you use extra features provided by Momentum, you are encouraged to use syntax like
`if (doesSdkSupport(["feature-name"])) { ... }` so that your JS app can work on Official
Firmware too, aswell as all other compliant Custom Firmwares. If some of those extra
features are essential to the functionality of your app, you can use `checkSdkFeatures(["feature1", "feature2"])`
near the beginning of your script, which will show a warning to the user that these features
are not available in their firmware distribution.

## Getting started
Create your application using the interactive wizard:
```shell
npx @next-flip/create-fz-app-mntm@latest
```

Then, enter the directory with your application and launch it:
```shell
cd my-flip-app
npm start
```

You are free to use `pnpm` or `yarn` instead of `npm`.

## Versioning
For each version of this package, the major and minor components match those of
the Flipper Zero JS SDK version that that package version targets. This version
follows semver. For example, apps compiled with SDK version `0.1.0` will be
compatible with SDK versions `0.1`...`1.0` (not including `1.0`).

Every API has a version history reflected in its JSDoc comment. It is heavily
recommended to check SDK compatibility using a combination of
`sdkCompatibilityStatus`, `isSdkCompatible`, `assertSdkCompatibility` depending
on your use case.

## Documentation
Check out the [JavaScript section in the Developer Documentation](https://developer.flipper.net/flipperzero/doxygen/js.html)
