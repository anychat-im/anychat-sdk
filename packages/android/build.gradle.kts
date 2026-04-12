plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
    id("signing")
}

android {
    namespace = "com.anychat.sdk"
    compileSdk = 34

    defaultConfig {
        minSdk = 24
        targetSdk = 34

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++23", "-frtti", "-fexceptions")
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_PLATFORM=android-24",
                    "-DBUILD_ANDROID_BINDING=ON"
                )
            }
        }

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    kotlinOptions {
        jvmTarget = "1.8"
    }

    sourceSets {
        getByName("main") {
            kotlin.srcDirs("src/main/kotlin")
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")

    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}

// Maven Publishing Configuration
afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("release") {
                from(components["release"])

                groupId = "io.github.yzhgit"
                artifactId = "anychat-sdk-android"
                version = findProperty("VERSION_NAME")?.toString() ?: "0.1.0"

                pom {
                    name.set("AnyChat Android SDK")
                    description.set("Official Android SDK for AnyChat instant messaging system")
                    url.set("https://github.com/yzhgit/anychat-sdk")

                    licenses {
                        license {
                            name.set("MIT License")
                            url.set("https://opensource.org/licenses/MIT")
                        }
                    }

                    developers {
                        developer {
                            id.set("yzhgit")
                            name.set("AnyChat Team")
                            email.set("yzhgit@users.noreply.github.com")
                        }
                    }

                    scm {
                        connection.set("scm:git:git://github.com/yzhgit/anychat-sdk.git")
                        developerConnection.set("scm:git:ssh://github.com/yzhgit/anychat-sdk.git")
                        url.set("https://github.com/yzhgit/anychat-sdk")
                    }
                }
            }
        }

        repositories {
            maven {
                name = "sonatype"
                val releasesRepoUrl = uri("https://s01.oss.sonatype.org/service/local/staging/deploy/maven2/")
                val snapshotsRepoUrl = uri("https://s01.oss.sonatype.org/content/repositories/snapshots/")
                url = if (version.toString().endsWith("SNAPSHOT")) snapshotsRepoUrl else releasesRepoUrl

                credentials {
                    username = findProperty("OSSRH_USERNAME")?.toString() ?: System.getenv("OSSRH_USERNAME")
                    password = findProperty("OSSRH_PASSWORD")?.toString() ?: System.getenv("OSSRH_PASSWORD")
                }
            }
        }
    }
}

// Signing configuration (uncomment when ready to publish)
// signing {
//     // Use GPG agent or provide signing key via gradle.properties
//     sign(publishing.publications["release"])
// }
