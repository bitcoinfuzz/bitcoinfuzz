name := "lightning-invoice-parser"
version := "0.1"
scalaVersion := "2.13.10"

// Generate JAR with dependencies
assembly / assemblyJarName := "lightning-invoice-parser.jar"

// Resolve merge conflicts in assembly
assembly / assemblyMergeStrategy := {
  case PathList("META-INF", xs @ _*) => MergeStrategy.discard
  case x => MergeStrategy.first
}
