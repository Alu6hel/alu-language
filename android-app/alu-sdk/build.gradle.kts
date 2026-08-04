plugins {
  id("com.android.library")
  id("maven-publish")
}

android {
    namespace = "com.example.alu.sdk"
    compileSdk = 36
    defaultConfig {
        minSdk = 24
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    jvmToolchain(17)
}

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("release") {
                from(components["release"])
                groupId = "com.example.alu"
                artifactId = "alu-sdk"
                version = "1.0.0"
            }
        }
    }
}
