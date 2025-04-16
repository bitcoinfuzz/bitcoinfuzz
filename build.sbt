name := "lightning-invoice-parser"
version := "0.1"
scalacOptions += "-release:11"

javacOptions ++= Seq("-source", "21", "-target", "21")
scalacOptions ++= Seq("-release", "21")

ThisBuild / scalaVersion := "2.13.12"

resolvers ++= Seq(
  Resolver.typesafeIvyRepo("releases"),
  "Sonatype OSS Snapshots" at "https://oss.sonatype.org/content/repositories/snapshots",
  "Sonatype OSS Releases" at "https://oss.sonatype.org/content/repositories/releases",
  Resolver.mavenLocal
)

unmanagedBase := baseDirectory.value / "lib"

libraryDependencies ++= Seq(
  "org.scala-lang" % "scala-library" % scalaVersion.value
)

assembly / assemblyJarName := "lightning-invoice-parser.jar"

assembly / assemblyMergeStrategy := {
  case PathList("META-INF", xs @ _*) => MergeStrategy.discard
  case x => MergeStrategy.first
}
