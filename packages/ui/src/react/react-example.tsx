/** @jsxImportSource react */
//This is used for testing whether the Qwik React components are working
import { useState } from "react"
import { qwikify$ } from "@builder.io/qwik-react"
import { motion } from "framer-motion"

export const ReactExample = () => {
    const [count, setCount] = useState(0)
    return (
        <div>
           
            <motion.div>
                Hello lovers
            </motion.div>
            {/* <motion.div
                style={{
                    width: "100px",
                    height: "100px",
                    backgroundColor: "red",
                }}
                animate={{y:0}}
                initial={{y:100}}
                // whileHover={{ scale: 1.2, rotate: 90 }}
                // whileTap={{
                //     scale: 0.8,
                //     rotate: -90,
                //     borderRadius: "100%"
                // }}
            /> */}
            Hello lovers
        </div>
    )
}

export const Example = qwikify$(ReactExample)