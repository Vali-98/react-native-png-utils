import { keepLocalCopy, pick } from '@react-native-documents/picker'
import { getPngChunkText } from '@vali98/react-native-png-utils'
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
    const pngtext = getPngChunkText(fileData)
    const after = performance.now() - now
    setTime(after)
    try {
      console.log(JSON.parse(pngtext.trim()))
      setText(JSON.stringify(JSON.parse(pngtext)))
    } catch (e) {
      setText(e + pngtext)
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

      <Text style={styles.text}>{time ?? 'None'}</Text>
    </SafeAreaView>
  )
}

const styles = StyleSheet.create({
  button: {
    fontSize: 40,
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
