import { keepLocalCopy, pick } from '@react-native-documents/picker'
import {
  extractPngTextChunk,
  replacePngTextChunk,
} from '@vali98/react-native-png-utils'
import React, { useState } from 'react'
import { ScrollView, StyleSheet, Text, TouchableOpacity } from 'react-native'
import { FileSystem } from 'react-native-file-access'
import { SafeAreaView } from 'react-native-safe-area-context'
function App(): React.JSX.Element {
  const [text, setText] = useState('')
  const [time, setTime] = useState(0)
  const handlePick = async () => {
    setText('')
    const [result] = await pick({
      allowMultiSelection: false,
    })
    if (!result) return
    const [file] = await keepLocalCopy({
      files: [{ fileName: 'output.png', uri: result.uri }],
      destination: 'documentDirectory',
    })
    if (file.status === 'error') return
    const fileData = await FileSystem.readFile(
      file.localUri.replace('file://', ''),
      'base64'
    )
    const now = performance.now()
    const pngtext = extractPngTextChunk(fileData)

    const after = performance.now() - now
    setTime(after)
    try {
      const results = JSON.stringify(pngtext)
      console.log(results)
      setText(results)
    } catch (e) {
      console.log(pngtext)
      console.log(e)
    }
  }

  const handleReadAndReplace = async () => {
    const [result] = await pick({
      allowMultiSelection: false,
    })
    if (!result) return
    const [file] = await keepLocalCopy({
      files: [{ fileName: 'output.png', uri: result.uri }],
      destination: 'documentDirectory',
    })
    if (file.status === 'error') return
    const fileData = await FileSystem.readFile(
      file.localUri.replace('file://', ''),
      'base64'
    )
    try {
      const newimage = replacePngTextChunk(fileData, [
        { data: 'Hello', keyword: 'hi' },
      ])

      const newpngtext = extractPngTextChunk(newimage)
      const result2 = newpngtext
      console.log(JSON.stringify(result2))
      setText(JSON.stringify(JSON.stringify(result2)))
    } catch (e) {
      setText('error' + e)
    }
  }

  return (
    <SafeAreaView style={styles.container} edges={['top', 'bottom']}>
      <ScrollView>
        <Text style={styles.text}>{text}</Text>
      </ScrollView>
      <TouchableOpacity onPress={handlePick}>
        <Text style={styles.button}>Pick</Text>
      </TouchableOpacity>
      <TouchableOpacity onPress={handleReadAndReplace}>
        <Text style={styles.button}>Read and Replace</Text>
      </TouchableOpacity>
      <Text style={styles.text}>{time ?? 'None'}</Text>
    </SafeAreaView>
  )
}

const styles = StyleSheet.create({
  button: {
    fontSize: 24,
    color: 'orange',
  },

  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: 80,
  },
  text: {
    color: 'green',
  },
})

export default App
